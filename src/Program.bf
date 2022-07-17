using Nanoforge.Render;
using Nanoforge.Misc;
using Nanoforge.App;
using Nanoforge.Gui;
using Nanoforge;
using Win32;
using System;

namespace Nanoforge
{
	public class Program
	{
		public static void Main(String[] args)
		{
            //TODO: Fix needing to manually set this. Should auto set using build system
#if DEBUG
            StringView assetsBasePath = "C:/Users/moneyl/source/repos/Nanoforge/assets/";
#else
            StringView assetsBasePath = "./assets/";
#endif

            App.Build!(AppState.Running)
                ..AddResource<BuildConfig>(new .("Project 80", assetsBasePath))
                ..AddSystem<Window>(isResource: true)
                ..AddSystem<Input>(isResource: true)
                ..AddSystem<Renderer>()
                ..AddSystem<Gui>(isResource: true)
				.Run();
		}
	}
}