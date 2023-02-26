using Nanoforge.App;
using Common;
using System;
using ImGui;
using System.Collections;
using Nanoforge.Gui.Documents;
using System.Linq;

namespace Nanoforge.Gui.Panels
{
    public class MainMenuBar : GuiPanelBase
    {
        private ImGui.DockNodeFlags dockspaceFlags = 0;
        public List<MenuItem> MenuItems = new .() ~ DeleteContainerAndItems!(_);
        public ImGui.ID DockspaceId = 0;
        public ImGui.ID DockspaceCentralNodeId = 0;
        bool ShowImGuiDemo = true;

        public static StringView[?] MapList = .("terr01", "dlc01", "mp_cornered", "mp_crashsite", "mp_crescent", "mp_crevice", "mp_deadzone", "mp_downfall", "mp_excavation", "mp_fallfactor",
            "mp_framework", "mp_garrison", "mp_gauntlet", "mp_overpass", "mp_pcx_assembly", "mp_pcx_crossover", "mp_pinnacle", "mp_quarantine",
            "mp_radial", "mp_rift", "mp_sandpit", "mp_settlement", "mp_warlords", "mp_wasteland", "mp_wreckage", "mpdlc_broadside", "mpdlc_division", "mpdlc_islands",
            "mpdlc_landbridge", "mpdlc_minibase", "mpdlc_overhang", "mpdlc_puncture", "mpdlc_ruins", "wc1", "wc2", "wc3", "wc4", "wc5", "wc6", "wc7", "wc8", "wc9",
            "wc10", "wcdlc1", "wcdlc2", "wcdlc3", "wcdlc4", "wcdlc5", "wcdlc6", "wcdlc7", "wcdlc8", "wcdlc9");

        public override void Update(App app, Gui gui)
        {
            static bool firstDraw = true;
            if (firstDraw)
            {
                GenerateMenus(gui);
                firstDraw = false;
            }

            DrawMainMenuBar(app, gui);
            DrawDockspace(app, gui);
            if (ShowImGuiDemo)
	            ImGui.ShowDemoWindow(&ShowImGuiDemo);
        }

        private void DrawMainMenuBar(App app, Gui gui)
        {
            FrameData frameData = app.GetResource<FrameData>();
            const f32 mainMenuContentStartY = 5.0f;

            if (ImGui.BeginMainMenuBar())
            {
                if (ImGui.BeginMenu("File"))
                {
                    if (ImGui.MenuItem("New project... (NOT IMPLEMENTED YET)"))
					{

					}
                    if (ImGui.MenuItem("Open project... (NOT IMPLEMENTED YET)"))
					{

					}
                    if (ImGui.MenuItem("Save project (NOT IMPLEMENTED YET)"))
					{

					}
                    if (ImGui.MenuItem("Close project (NOT IMPLEMENTED YET)"))
					{

					}
                    if (ImGui.BeginMenu("Recent projects (NOT IMPLEMENTED YET)"))
                    {

                        ImGui.EndMenu();
                    }

                    ImGui.Separator();

                    if (ImGui.MenuItem("Save file (NOT IMPLEMENTED YET)", "Ctrl + S"))
                    {

                    }

                    ImGui.Separator();

                    if (ImGui.MenuItem("Settings (NOT IMPLEMENTED YET)"))
                    {

                    }

                    if (ImGui.MenuItem("Exit"))
					{
                        gui.CloseNanoforgeRequested = true;
                        for (GuiDocumentBase doc in gui.Documents)
                            doc.Open = false; //Close all documents so save confirmation modal appears for them
				    }
                    ImGui.EndMenu();
                }

                //Draw menu item for each panel (e.g. file explorer, properties, log, etc) so user can toggle visibility
                for (MenuItem item in MenuItems)
                    item.Draw();

                if (ImGui.BeginMenu("View"))
                {
                    ImGui.MenuItem(gui.OutlinerIdentifier, "", &gui.OutlinerOpen);
                    ImGui.MenuItem(gui.InspectorIdentifier, "", &gui.InspectorOpen);
                    ImGui.EndMenu();
                }
                //ImGui.TextEx(Icons.ICON_FA_MAP);
                ImGui.SetNextWindowSize(.(-1.0f, 600.0f));
                if (ImGui.BeginMenu("Maps"))
                {
                    for (StringView map in MapList)
                    {
                        bool supported = map != "terr01" && map != "dlc01";
                        bool alreadyOpen = gui.Documents.Any((doc) => doc.Title == map);
                        if (ImGui.MenuItem(supported ? map : scope $"{map} (SP maps not supported yet)", "", null, supported && !alreadyOpen))
                        {
                            gui.OpenDocument(map, map, new MapEditorDocument(map));
                        }
                    }
                    ImGui.EndMenu();
                }
                if (ImGui.BeginMenu("Help"))
                {
                    if (ImGui.MenuItem("Welcome")) { }
                    if (ImGui.MenuItem("Metrics")) { }
                    if (ImGui.MenuItem("About")) { }
                    ImGui.EndMenu();
                }

                var drawList = ImGui.GetWindowDrawList();
                String realFrameTime = scope String()..AppendF("{0:G3}", frameData.AverageFrameTime * 1000.0f);
                String totalFrameTime = scope String()..AppendF("/  {0:G4}", frameData.DeltaTime * 1000.0f);
                drawList.AddText(.(ImGui.GetCursorPosX(), 5.0f), 0xF2F5FAFF, "|    Frametime (ms): ");
                var textSize = ImGui.CalcTextSize("|    Frametime (ms): ");
                drawList.AddText(.(ImGui.GetCursorPosX() + (f32)textSize.x, 5.0f), ImGui.ColorConvertFloat4ToU32(ImGui.SecondaryTextColor), realFrameTime.CStr());
                drawList.AddText(.(ImGui.GetCursorPosX() + (f32)textSize.x + 42.0f, 5.0f), ImGui.ColorConvertFloat4ToU32(ImGui.SecondaryTextColor), totalFrameTime.CStr());

                //Draw NF version on the right side
                ImGui.Vec2 cursorPos = .();
                {
                    String version = BuildConfig.Version;
                    f32 versionTextWidth = ImGui.CalcTextSize(version.CStr()).x;
                    f32 padding = 8.0f;
                    f32 rightPadding = -0.0f;
                    cursorPos = .(ImGui.GetWindowWidth() - versionTextWidth - padding - rightPadding, mainMenuContentStartY);
                    drawList.AddText(cursorPos, 0xF2F5FAFF, version.CStr(), version.CStr() + version.Length);
                }

                ImGui.EndMainMenuBar();
            }
        }

        private void DrawDockspace(App app, Gui gui)
        {
            //Dockspace flags
            dockspaceFlags = ImGui.DockNodeFlags.None;

            //Parent window flags
            ImGui.WindowFlags windowFlags = .NoDocking | .NoTitleBar | .NoCollapse | .NoResize | .NoMove | .NoBringToFrontOnFocus | .NoNavFocus | .NoBackground;
            var viewport = ImGui.GetMainViewport();

            //Set dockspace size and params
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
            if ((io.ConfigFlags & .DockingEnable) != 0)
            {
                bool firstDraw = DockspaceId == 0;
                DockspaceId = ImGui.GetID("Editor dockspace");
                if (firstDraw)
                {
                    ImGui.DockBuilderRemoveNode(DockspaceId);
                    ImGui.DockBuilderAddNode(DockspaceId, (ImGui.DockNodeFlags)ImGui.DockNodeFlagsPrivate.DockNodeFlags_DockSpace);
                    ImGui.DockNode* dockspaceNode = ImGui.DockBuilderGetNode(DockspaceId);
                    dockspaceNode.LocalFlags |= (ImGui.DockNodeFlags)(ImGui.DockNodeFlagsPrivate.DockNodeFlags_NoWindowMenuButton | ImGui.DockNodeFlagsPrivate.DockNodeFlags_NoCloseButton); //Disable extra close button on dockspace. Tabs will still have their own.
                    ImGui.DockBuilderFinish(DockspaceId);
                }
                ImGui.DockSpace(DockspaceId, .(0.0f, 0.0f), dockspaceFlags);
            }

            ImGui.End();

            //Set default docking positions on first draw
            if (FirstDraw)
            {
                ImGui.ID dockLeftId = ImGui.DockBuilderSplitNode(DockspaceId, .Left, 0.20f, var outIdLeft, out DockspaceId);
                ImGui.ID dockRightId = ImGui.DockBuilderSplitNode(DockspaceId, .Right, 0.28f, var outIdRight, out DockspaceId);
                //ImGui.ID dockRightUp = ImGui.DockBuilderSplitNode(dockRightId, .Up, 0.35f, var outIdRightUp, out dockRightId);
                DockspaceCentralNodeId = ImGui.DockBuilderGetCentralNode(DockspaceId).ID;
                ImGui.ID dockCentralDownSplitId = ImGui.DockBuilderSplitNode(DockspaceCentralNodeId, .Down, 0.20f, var outIdCentralDown, out DockspaceCentralNodeId);

                //Todo: Tie panel titles to these calls so both copies of the strings don't need to be updated every time the code changes
                ImGui.DockBuilderDockWindow("Start page", DockspaceCentralNodeId);
                ImGui.DockBuilderDockWindow("File explorer", dockLeftId);
                ImGui.DockBuilderDockWindow("Dear ImGui Demo", dockLeftId);
                ImGui.DockBuilderDockWindow(StateViewer.ID.Ptr, dockLeftId);
                ImGui.DockBuilderDockWindow(gui.OutlinerIdentifier, dockLeftId);
                ImGui.DockBuilderDockWindow(gui.InspectorIdentifier, dockRightId);
                ImGui.DockBuilderDockWindow("Log", dockCentralDownSplitId);
            }
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
        public String Text = new .() ~ delete _;
        public List<MenuItem> Items = new .() ~ DeleteContainerAndItems!(_);
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