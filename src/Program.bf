using Nanoforge.Systems;
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
            StringView assetsBasePath = "C:/Users/lukem/source/repos/Nanoforge/assets/";
#else
            StringView assetsBasePath = "./assets/";
#endif
            App.Build!(AppState.Running)
                ..AddResource<BuildConfig>(new .("Nanoforge", assetsBasePath, "v1.0.0"))
                ..AddSystem<Window>(isResource: true)
                ..AddSystem<Input>(isResource: true)
                ..AddSystem<Renderer>()
                ..AddSystem<Gui>(isResource: true)
                ..AddSystem<AppLogic>()
				.Run();
		}
	}
}