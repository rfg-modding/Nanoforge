using Nanoforge.Systems;
using Nanoforge.Render;
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
            RfgTools.Hashing.HashDictionary.Initialize();

            App.Build!(AppState.Running)
                ..AddSystem<Window>(isResource: true)
                ..AddSystem<Input>(isResource: true)
                ..AddSystem<Renderer>(isResource: true)
                ..AddSystem<Gui>(isResource: true)
                ..AddSystem<AppLogic>()
				.Run();
		}
	}
}