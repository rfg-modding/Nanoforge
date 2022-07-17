using Nanoforge.Misc.Containers;
using System.Collections;
using System.Diagnostics;
using System.Reflection;
using Nanoforge;
using System;

namespace Nanoforge.App
{
	public class SystemFunction
	{
		private Object _system;
		private function void(Object system, App app) _function;
        private append Stopwatch _timer = .(false);

        public const int PROFILING_BUFFER_SIZE = 10;
        public append RingBuffer<f32, const PROFILING_BUFFER_SIZE> RunTimes = .(); //
		public readonly StringView Namespace;
		public readonly StringView ClassName;
		public readonly StringView Name;
		public mixin FullName() { scope:: $"{Namespace}.{ClassName}.{Name}()" }

		public this(Object system, MethodInfo method)
		{
			_system = system;
			_function = (function void(Object system, App app))method.[Friend]mData.mMethodData.mFuncPtr;

			//Store names for debug reasons
			Namespace = method.[Friend]mTypeInstance.[Friend]mNamespace;
			ClassName = method.[Friend]mTypeInstance.[Friend]mName;
			Name = method.Name;
		}

		public void Invoke(App app)
		{
            _timer.Restart();
			_function(_system, app);
            RunTimes.Push((f32)_timer.ElapsedMicroseconds / 1000000.0f);
		}

        public f32 AverageRunTime()
        {
            f32 total = 0.0f;
            for (f32 value in RunTimes.[Friend]_data) //TODO: Create some kind of readonly enumerator to read values without changing
                total += value;

            //TODO: Track actual element count along with capacity. Not a big deal here since the buffer gets filled quickly with 60+ FPS
            return total / RunTimes.Size;
        }
	}

	//The type of state transition/status a system needs to run. See SystemSet comments for info on what each means.
	public enum SystemRunType
	{
		Active, //State is at the top of the state stack
		Paused, //State is on the state stack but not the top
		Always, //State is anywhere on the stack
		OnPause, //Ran once when the system is on top of the stack and another state is pushed onto it, pausing it
		OnResume, //Ran once when the state above this one is popped from the stack making this the primary state
		OnEnter, //Ran when the state is pushed onto the stack
		OnExit //Ran when the state is popped off of the stack
	}

	//A set of ECS systems to run when game state changes
	public class SystemSet
	{
		//These systems will only run when this state is active
		public readonly AppState State; //Equivalent to a game state enum defined on the game side of things.

		//List of systems to be run. Index is the StateTransitionType that the systems should be run on
		public List<SystemFunction>[Enum.Count<SystemRunType>()] Systems;

        //Get system by transition type
        public List<SystemFunction> GetSystems(SystemRunType type) => Systems[(u32)type];

		public this(AppState state)
		{
			State = state;
			for (u32 i = 0; i < Systems.Count; i++)
				Systems[i] = new List<SystemFunction>();
		}

        public ~this()
        {
            for (List<SystemFunction> systemList in Systems)
                DeleteContainerAndItems!(systemList);
        }
	}
}
