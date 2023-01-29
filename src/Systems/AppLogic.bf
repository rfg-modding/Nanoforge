using Nanoforge.FileSystem;
using Nanoforge.Misc;
using Nanoforge.App;
using System;

namespace Nanoforge.Systems
{
	[System]
	class AppLogic : ISystem
	{
        //TODO: De-hardcode this. Add a data folder selector UI + auto game detection like the C++ version had.
        public static StringView DataFolderPath = "D:/GOG/Games/Red Faction Guerrilla Re-Mars-tered/data/";

		static void ISystem.Build(App app)
		{

		}

		[SystemInit]
		void Init(App app)
		{
            PackfileVFS.SetDataFolder(DataFolderPath);
		}

		[SystemStage]
		void Update(App app)
		{

		}

        [SystemStage(.EndFrame)]
        void EndFrame(App app)
        {
            Events.EndFrame();
        }
	}
}