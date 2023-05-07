using Nanoforge.FileSystem;
using Nanoforge.Misc;
using Nanoforge.App;
using System;

namespace Nanoforge.Systems
{
	[System]
	class AppLogic : ISystem
	{
		static void ISystem.Build(App app)
		{

		}

		[SystemInit]
		void Init(App app)
		{
            ProjectDB.Init(); //TODO: Tried using StaticInitOrder + constructor for this, but it wouldn't init buffer Logger even if I told it to. Do it that way once that Beef bug is fixed.
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