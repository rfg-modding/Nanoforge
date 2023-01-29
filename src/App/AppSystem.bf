using Nanoforge.Misc;
using Nanoforge.App;
using Common;
using System;

namespace Nanoforge.App
{
	//Each component field that has this attribute will show up in the entity editor
	[AttributeUsage(.Method | .MemberAccess, .ReflectAttribute, ReflectUser=.Methods)]
	public struct SystemStageAttribute : Attribute
	{
		public AppState State; //By default the system runs in all states
	    public readonly SystemStage Stage;
		public readonly SystemRunType RunType;

		//TODO: Get this working as 1 constructor with optional args. Initially tried this but it reflection was detecting the attributes unless all arguments were specified
		public this()
		{
			Stage = .Update;
			RunType = .Always;
			State = .None;
		}

		public this(SystemStage stage)
		{
		    Stage = stage;
			RunType = .Always;
			State = .None;
		}

		public this(SystemStage stage, SystemRunType runType)
		{
		    Stage = stage;
			RunType = runType;
			State = .None;
		}

	    public this(SystemStage stage, SystemRunType runType, AppState appState)
	    {
	        Stage = stage;
			RunType = runType;
			State = appState;
	    }
	}

	[AttributeUsage(.Method | .MemberAccess, .ReflectAttribute, ReflectUser=.Methods)]
	public struct SystemInitAttribute : Attribute
	{
		public this()
		{

		}
	}

	public struct SystemAttribute : ReflectAllAttribute
	{

	}

	public interface ISystem
	{
		public static void Build(App app);
	}
}