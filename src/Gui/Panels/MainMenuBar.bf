using Nanoforge.App;
using Nanoforge;
using System;
using ImGui;
using System.Collections;

namespace Nanoforge.Gui.Panels
{
	public class MainMenuBar : GuiPanelBase
	{
		private ImGui.DockNodeFlags dockspaceFlags = 0;
        public List<MenuItem> MenuItems = new .() ~DeleteContainerAndItems!(_);

		public override void Update(App app, Gui gui)
		{
            static bool firstDraw = true;
            if (firstDraw)
            {
                GenerateMenus(gui);
                firstDraw = false;
            }

			DrawMainMenuBar(app);
			DrawDockspace(app);
		}

		private void DrawMainMenuBar(App app)
		{
			FrameData frameData = app.GetResource<FrameData>();

			if (ImGui.BeginMainMenuBar())
			{
				if(ImGui.BeginMenu("File"))
				{
					if(ImGui.MenuItem("Open file...")) { }
					if(ImGui.MenuItem("Save file...")) { }
					if(ImGui.MenuItem("Exit")) { }
					ImGui.EndMenu();
				}
				if(ImGui.BeginMenu("Edit"))
				{
					ImGui.EndMenu();
				}

                //Draw menu item for each panel (e.g. file explorer, properties, log, etc) so user can toggle visibility
                for (MenuItem item in MenuItems)
                    item.Draw();

				if(ImGui.BeginMenu("View"))
				{
					//Todo: Put toggles for other guis in this layer here
					ImGui.EndMenu();
				}
				if(ImGui.BeginMenu("Help"))
				{
					if(ImGui.MenuItem("Welcome")) { }
					if(ImGui.MenuItem("Metrics")) { }
					if(ImGui.MenuItem("About")) { }
					ImGui.EndMenu();
				}

				var drawList = ImGui.GetWindowDrawList();
				String realFrameTime = scope String()..AppendF("{0:G3}", frameData.AverageFrameTime * 1000.0f);
                String totalFrameTime = scope String()..AppendF("/  {0:G4}", frameData.DeltaTime * 1000.0f);
				drawList.AddText(.(ImGui.GetCursorPosX(), 5.0f), 0xF2F5FAFF, "|    Frametime (ms): ");
				var textSize = ImGui.CalcTextSize("|    Frametime (ms): ");
				drawList.AddText(.(ImGui.GetCursorPosX() + (f32)textSize.x, 5.0f), ImGui.ColorConvertFloat4ToU32(ImGui.SecondaryTextColor), realFrameTime.CStr());
                drawList.AddText(.(ImGui.GetCursorPosX() + (f32)textSize.x + 42.0f, 5.0f), ImGui.ColorConvertFloat4ToU32(ImGui.SecondaryTextColor), totalFrameTime.CStr());

				ImGui.EndMainMenuBar();
			}
		}

		private void DrawDockspace(App app)
		{
			//Dockspace flags
			dockspaceFlags = ImGui.DockNodeFlags.None;

			//Parent window flags
			ImGui.WindowFlags windowFlags = .NoDocking | .NoTitleBar | .NoCollapse | .NoResize | .NoMove | .NoBringToFrontOnFocus | .NoNavFocus | .NoBackground;

			//Set dockspace size and params
			var viewport = ImGui.GetMainViewport();
			ImGui.SetNextWindowPos(viewport.WorkPos);
			var dockspaceSize = viewport.Size;
			ImGui.SetNextWindowSize(dockspaceSize);
			ImGui.SetNextWindowViewport(viewport.ID);
			ImGui.PushStyleVar(ImGui.StyleVar.WindowRounding, 0.0f);
			ImGui.PushStyleVar(ImGui.StyleVar.WindowBorderSize, 0.0f);
			ImGui.PushStyleVar(ImGui.StyleVar.WindowPadding, .(0.0f, 0.0f));
			ImGui.Begin("Dockspace parent window", null, windowFlags);
			ImGui.PopStyleVar(3);

			//Create dockspace
			var io = ImGui.GetIO();
			if ((io.ConfigFlags & ImGui.ConfigFlags.DockingEnable) != 0)
			{
			    ImGui.ID dockspaceId = ImGui.GetID("Editor dockspace");
			    ImGui.DockSpace(dockspaceId, .(0.0f, 0.0f), dockspaceFlags);
			}

			ImGui.End();
		}

        private MenuItem GetMenu(StringView text)
        {
            for (MenuItem item in MenuItems)
                if (StringView.Equals(item.Text, text, true))
                    return item;

            return null;
        }

        private void GenerateMenus(Gui gui)
        {
            for (GuiPanelBase panel in gui.Panels)
            {
                //No menu entry if it's left blank. The main menu itself also doesn't have an entry
                if (panel.MenuPos == "" || panel == this)
                {
                    panel.Open = true;
                    continue;
                }

                //Split menu path into components
                StringView[] pathSplit = panel.MenuPos.Split!('/');
                StringView menuName = pathSplit[0];

                //Get or create menu
                MenuItem curMenuItem = GetMenu(menuName);
                if (curMenuItem == null)
                {
                    curMenuItem = new MenuItem(menuName);
                    MenuItems.Add(curMenuItem);
                }

                //Iterate through path segmeents to create menu tree
                for (int i = 1; i < pathSplit.Count; i++)
                {
                    StringView nextPart = pathSplit[i];
                    MenuItem nextItem = curMenuItem.GetChild(nextPart);
                    if (nextItem == null)
                    {
                        nextItem = new MenuItem(nextPart);
                        curMenuItem.Items.Add(nextItem);
                    }

                    curMenuItem = nextItem;
                }

                curMenuItem.Panel = panel;
            }
        }
	}

    //Entry in the main menu bar
    public class MenuItem
    {
        public String Text = new .() ~delete _;
        public List<MenuItem> Items = new .() ~DeleteContainerAndItems!(_);
        public GuiPanelBase Panel = null;

        public this(StringView text)
        {
            Text.Set(text);
        }

        public void Draw()
        {
            if (Panel != null)
            {
                ImGui.MenuItem(Text, "", &Panel.Open);
                return;
            }

            if (ImGui.BeginMenu(Text))
            {
                for (MenuItem item in Items)
                {
                    item.Draw();
                }
                ImGui.EndMenu();
            }
        }

        public MenuItem GetChild(StringView text)
        {
            for (MenuItem item in Items)
                if (StringView.Equals(item.Text, text, true))
                    return item;

            return null;
        }
    }
}