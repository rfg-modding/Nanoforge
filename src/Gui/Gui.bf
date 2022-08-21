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
        public List<GuiPanelBase> Panels = new .() ~DeleteContainerAndItems!(_);

		static void ISystem.Build(App app)
		{

		}

		[SystemInit]
		void Init(App app)
		{
            AddPanel("", true, new MainMenuBar());
            AddPanel("View/Debug", true, new DebugGui());
            AddPanel("View/State viewer", true, new StateViewer());
		}

		[SystemStage(.Update)]
		void Update(App app)
		{
			for (GuiPanelBase panel in Panels)
                if (panel.Open)
                    panel.Update(app, this);
		}

        ///Add gui panel and validate its path to make sure its not a duplicate. Takes ownership of panel.
        void AddPanel(StringView menuPos, bool open, GuiPanelBase panel)
        {
            //Make sure there isn't already a panel with the same menuPos
            for (GuiPanelBase existingPanel in Panels)
            {
                if (StringView.Equals(menuPos, existingPanel.MenuPos, true))
                {
                    //Just crash when a duplicate is found. These are set at compile time currently so it'll always happen if theres a duplicate
                    Runtime.FatalError(scope $"Duplicate GuiPanel menu path. Fix this before release. Type = {panel.GetType().ToString(.. scope .())}. Path = '{menuPos}'");
                    delete panel;
                    return;
                }
            }

            panel.MenuPos.Set(menuPos);
            panel.Open = open;
            Panels.Add(panel);
        }
	}
}