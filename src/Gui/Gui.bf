using System.Collections;
using Nanoforge.Gui.Panels;
using Nanoforge.Math;
using Nanoforge.App;
using Nanoforge;
using System;
using ImGui;

namespace Nanoforge.Gui
{
	[System]
	public class Gui : ISystem
	{
        public List<IGuiPanel> Panels = new .() ~DeleteContainerAndItems!(_);

		static void ISystem.Build(App app)
		{

		}

		[SystemInit]
		void Init(App app)
		{
            Panels.Add(new MainMenuBar());
            Panels.Add(new DebugGui());
            Panels.Add(new StateViewer());
		}

		[SystemStage(.Update)]
		void Update(App app)
		{
			for (IGuiPanel panel in Panels)
                panel.Update(app);
		}
	}
}