using Nanoforge.Misc;
using Nanoforge.App;
using Nanoforge;
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