using Nanoforge.Misc.Containers;
using System.Collections;
using System.Diagnostics;
using System.Reflection;
using Nanoforge.Misc;
using Common.Math;
using Common;
using System;

namespace Nanoforge.App
{
    [ReflectAll]
    public enum AppState
    {
        None,
        Running
    }

	public class FrameData
	{
        ///Amount of time this frame that work was being done. DeltaTime includes time waiting for target framerate. This doesn't.
        public f32 RealFrameTime;

        ///Total time elapsed last frame. Includes time waiting until target framerate if there was time to spare
		public f32 DeltaTime;

        ///Number of frames to sample AverageFrameTime
        public const int NumFrametimeSamples = 30;

        ///Last N real frametimes. Used to calculate AverageFrameTime
        public append RingBuffer<f32, 30> AverageFrametimeSamples;

        ///Average of last N RealFrameTime values. Use when you want a more stable value for UI purposes. The real value can fluctuate frame by frame
        public f32 AverageFrameTime
        {
		    get
    		{
                f32 result = 0.0f;
                for (f32 v in AverageFrametimeSamples.[Friend]_data) //TODO: Fix the RingBuffer enumerator
                    result += v;

                return result / AverageFrametimeSamples.Size;
		    }
		};

	}

	//System stages. Run each frame in this order.
	public enum SystemStage
	{
		BeginFrame,
		PreUpdate,
		Update,
		PostUpdate,
		EndFrame,
	}

	//A list of systems to be run at certain app states. Each index corresponds with the state value. Last index is independent systems.
	public class Stage
	{
		public append List<SystemSet> Sets ~ClearAndDeleteItems!(Sets);
		public append List<SystemFunction> IndependentSystems ~ClearAndDeleteItems!(IndependentSystems);

		public this()
		{
			for (u32 j = 0; j < Enum.Count<AppState>(); j++)
				Sets.Add(new SystemSet((AppState)j));
		}
	}

	public class App
	{
		//List of resources and list of their types. Only one instance of a resource can exist per type.
        private List<IResource> _resources = new .() ~DeleteContainerAndItems!(_resources);
        private List<Type> _resourceTypes = new .() ~delete _;
        private List<AppState> _stateStack = new .() ~delete _;
        private List<Stage> _stages = new .() ~DeleteContainerAndItems!(_stages);
        private List<Object> _systems = new .() ~DeleteContainerAndItems!(_systems);
        private List<SystemFunction> _startupSystems = new .() ~DeleteContainerAndItems!(_startupSystems);
        //TODO: Make framerate limiter configurable per system
        private u32 _maxFrameRate = 60;
        private f32 _maxFrameRateDelta = 1.0f / (float)_maxFrameRate;
        private append Stopwatch _frameTimer = .(false);
        public bool Exit = false;

		//Create a blank app ready for building. Call AddSystem<T>() and AddResource<T>() to build, then call .Run() to execute
		public static mixin Build(AppState initialState)
		{
			scope ::App(initialState)
		}

		public this(AppState initialState)
		{
			_stateStack.Add(initialState);
			for (u32 i = 0; i < Enum.Count<SystemStage>(); i++)
				_stages.Add(new .());

			AddResource<FrameData>();
		}

		//Run systems until exit is requested
		public void Run()
		{
			//Run startup systems once
			for (var startupSystem in ref _startupSystems)
			{
				startupSystem(this);
			}

			//Main loop
			while (!Exit)
			{
				FrameData frameData = GetResource<FrameData>();
                _frameTimer.Restart();

				//Run each stage. The order in the enum definition matches the order of execution
				for (u32 i = 0; i < Enum.Count<SystemStage>(); i++)
				{
					Stage stage = _stages[i];
					SystemSet set = GetStateSystems(stage, State());

					//Run independent systems
					for (var system in stage.IndependentSystems)
						system(this);

					//Run active systems
					for (var system in set.GetSystems(.Always))
						system(this);
					for (var system in set.GetSystems(.Active))
						system(this);

					//Run paused systems
					for (var set2 in stage.Sets)
						if (set2.State != State())
							for (var system in set2.GetSystems(.Paused))
								system(this);
				}

                //Wait until target frametime
                frameData.RealFrameTime = (f32)_frameTimer.ElapsedMicroseconds / 1000000.0f;
                frameData.AverageFrametimeSamples.Push(frameData.RealFrameTime);
                while(((float)_frameTimer.ElapsedMicroseconds / 1000000.0f) < _maxFrameRateDelta)
				{

				}
				frameData.DeltaTime = (f32)_frameTimer.ElapsedMicroseconds / 1000000.0f;
			}
		}

		//Register a resource type
		public void AddResource<T>(T value, bool isSystem = false) where T : class, delete
		{
			//Return early if resource type has already been registered
			var type = typeof(T);
			if (_resourceTypes.Contains(type))
				return;

			//Register resource and create instance of it
			_resources.Add(new Resource<T>(value, isSystem));
			_resourceTypes.Add(type);
		}

		//Register resource type using default constructor
		public void AddResource<T>(bool isSystem = false) where T : class, new, delete
		{
			//Return early if resource type has already been registered
			var type = typeof(T);
			if (_resourceTypes.Contains(type))
				return;

			//Register resource and create instance of it
			_resources.Add(new Resource<T>(new T(), isSystem));
			_resourceTypes.Add(type);
		}

		//Add system to app. System functions & the stage + state they're run at determined by attributes applied to member functions
		public void AddSystem<SystemType>(bool isResource = false) where SystemType : ISystem where SystemType : new, delete, class
		{
			Object system = _systems.Add(.. new SystemType());
			Type systemType = typeof(SystemType);

			//System is also a resource
			if (isResource)
				AddResource<SystemType>((SystemType)system, true);

			//Find system functions
			for (var method in systemType.GetMethods())
			{
				var maybeFunction = method.GetCustomAttribute<SystemStageAttribute>();
				var maybeStartupFunction = method.GetCustomAttribute<SystemInitAttribute>();

				//Function is a startup system. Gets run once when the app is started
				if (maybeStartupFunction case .Ok(let startupFunctionAttribute))
				{
					_startupSystems.Add(new .(system, method));
				}

				//Is it a system function
				if (maybeFunction case .Ok(let functionAttribute))
				{
					SystemStage runStage = functionAttribute.Stage;
					SystemRunType runType = functionAttribute.RunType;
					AppState gameState = functionAttribute.State;

					Stage stage = _stages[(int)runStage];
					if (gameState == .None) //Always run regardless of game state
					{
						stage.IndependentSystems.Add(new .(system, method));
					}
					else
					{
						SystemSet set = GetStateSystems(stage, gameState); //Set of systems that run on the given game state
						List<SystemFunction> funcs = set.Systems[(int)runType];
						funcs.Add(new .(system, method));
					}
				}
			}

			SystemType.Build(this);
		}

		//Current state (top of the stack)
		AppState State() => _stateStack.Back;

		SystemSet GetStateSystems(Stage stage, AppState state) => stage.Sets[(int)state];

		//Replace the state at the top of the state stack. Immediately runs transition systems.
		public void SetState(AppState state)
		{
			//******************
			//TODO: Possible bug in this and other stage functions: OnEnter and OnExit should be run in the respective stage in Update()

			//Run OnExit on old state and OnEnter on new state
			AppState oldState = State();
			for (u32 i = 0; i < Enum.Count<SystemStage>(); i++)
			{
				Stage stage = _stages[i];
				for (var system in GetStateSystems(stage, oldState).GetSystems(.OnExit))
					system(this);
				for (var system in GetStateSystems(stage, state).GetSystems(.OnEnter))
					system(this);
			}

			_stateStack.Back = state;
		}

		//Push state onto the state stack. Immediately runs transition systems.
		public void PushState(AppState state)
		{
			//Run OnPause on old state and OnEnter on new state
			AppState oldState = State();
			for (u32 i = 0; i < Enum.Count<SystemStage>(); i++)
			{
				Stage stage = _stages[i];
				for (var system in GetStateSystems(stage, oldState).GetSystems(.OnPause))
					system(this);
				for (var system in GetStateSystems(stage, state).GetSystems(.OnEnter))
					system(this);
			}

			_stateStack.Add(state);
		}

		//Pop a state off of the state stack. Immediately runs transition systems.
		public AppState PopState()
		{
			//Run OnExit systems for state and pop it from the stack
			AppState oldState = State();
			for (u32 i = 0; i < Enum.Count<SystemStage>(); i++)
			{
				Stage stage = _stages[i];
				for (var system in GetStateSystems(stage, oldState).GetSystems(.OnExit))
					system(this);
			}

			return _stateStack.PopBack();
		}

		public Result<T, void> GetResource<T>() where T : class, delete
		{
			Type type = typeof(T);
			for (IResource res in _resources)
				if (res.GetValueType() == type)
					return .Ok((T)res.Get());

			return .Err;
		}
	}
}
