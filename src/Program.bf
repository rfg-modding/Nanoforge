using Nanoforge.Systems;
using Nanoforge.Render;
using RfgTools.Hashing;
using Nanoforge.Misc;
using Nanoforge.App;
using Nanoforge.Gui;
using Common;
using System;
using Win32;

namespace Nanoforge
{
	public class Program
	{
		public static void Main(String[] args)
		{
            //Initialize utility for determining origin string of hashes used in game files
            HashDictionary.Initialize();

            //Used as workaround to static init problems. See comments on ICVar
            for (ICVar cvar in CVarsRequiringInitialization)
                cvar.Init();

            //The CVar static initializers call ProjectDB.LoadGlobals() during static init. If a global it gets created during that process too.
            //Here we check if any new ones were created and save them to the file.
            if (NanoDB.NewUnsavedGlobalObjects)
                NanoDB.SaveGlobals();

            App.Build!(AppState.Running)
                ..AddSystem<Window>(isResource: true)
                ..AddSystem<Input>(isResource: true)
                ..AddSystem<Gui>(isResource: true)
                ..AddSystem<Renderer>(isResource: true)
                ..AddSystem<AppLogic>()
				.Run();
		}
	}
}