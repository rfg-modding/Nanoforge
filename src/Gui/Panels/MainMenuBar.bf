using Nanoforge.App;
using Common;
using System;
using ImGui;
using System.Collections;
using Nanoforge.Gui.Documents;
using System.Linq;
using NativeFileDialog;
using Nanoforge.Misc;
using Nanoforge.Gui.Dialogs;
using Nanoforge.FileSystem;
using Xml_Beef;
using System.Threading;

namespace Nanoforge.Gui.Panels
{
    [ReflectAll]
    public class MainMenuBar : GuiPanelBase
    {
        private ImGui.DockNodeFlags dockspaceFlags = 0;
        public List<MenuItem> MenuItems = new .() ~ DeleteContainerAndItems!(_);
        public ImGui.ID DockspaceId = 0;
        public ImGui.ID DockspaceCentralNodeId = 0;
        bool ShowImGuiDemo = true;

        private class MapOption
        {
            public append String DisplayName;
            public append String FileName;

            public this(StringView displayName, StringView fileName)
            {
                DisplayName.Set(displayName);
                FileName.Set(fileName);
            }
        }
        private append Monitor _mapListLock;
        private append List<MapOption> _mpMaps ~ClearAndDeleteItems(_);
        private append List<MapOption> _wcMaps ~ClearAndDeleteItems(_);
        private bool _loadedMapLists = false;

        public this()
        {
            PackfileVFS.DataFolderChangedEvent.Add(new => this.OnDataFolderChanged);
        }

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
                    if (ImGui.MenuItem("New project..."))
					{
                        gui.CreateProject();
					}
                    if (ImGui.MenuItem("Open project..."))
					{
                        gui.OpenProject();
					}
                    if (ImGui.MenuItem("Save project", "Ctrl + S"))
					{
                        gui.SaveAll(app);
					}
                    if (ImGui.MenuItem("Close project"))
					{
                        gui.CloseProject();
					}
                    var recentProjects = CVar_GeneralSettings->RecentProjects;
                    if (ImGui.BeginMenu("Recent projects", recentProjects.Count > 0))
                    {
                        String projectToRemove = null;
                        for (String projectPath in recentProjects)
                        {
                            if (ImGui.MenuItem(projectPath..EnsureNullTerminator()))
                            {
                                NanoDB.LoadAsync(projectPath);
                            }
                            if (ImGui.BeginPopupContextItem())
                            {
                                if (ImGui.Selectable("Remove from list"))
                                {
                                    projectToRemove = projectPath; //Store reference to remove outside the loop. Shouldn't remove while iterating.
                                }

                                ImGui.EndPopup();
                            }
                        }

                        if (projectToRemove != null)
                        {
                            recentProjects.Remove(projectToRemove);
                            delete projectToRemove;
                            CVar_GeneralSettings.Save();
                        }

                        ImGui.EndMenu();
                    }

                    ImGui.Separator();

                    //Disabled for now since we don't have a way to save changes only for one document
                    /*if (ImGui.MenuItem("Save file", "Ctrl + S"))
                    {

                    }

                    ImGui.Separator();*/

                    if (ImGui.MenuItem("Settings...", "", false, false))
                    {

                    }
                    if (ImGui.MenuItem("Select data folder..."))
                    {
                        gui.DataFolderSelector.Show();
                        gui.DataFolderSelector.DataFolder.Set(PackfileVFS.DirectoryPath);
                    }

                    if (ImGui.MenuItem("Exit"))
					{
                        gui.CloseNanoforge();
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

                if (NanoDB.CurrentProject.Loaded && _loadedMapLists)
                {
                    if (ImGui.BeginMenu("Maps"))
                    {
                        ImGui.SetNextWindowSize(.(-1.0f, 600.0f));
                        if (ImGui.BeginMenu("Multiplayer"))
                        {
                            ScopedLock!(_mapListLock);
                            for (MapOption map in _mpMaps)
                            {
                                bool alreadyOpen = gui.Documents.Any((doc) => doc.Title == map.FileName);
                                if (ImGui.MenuItem(map.DisplayName, scope $"{map.FileName}.vpp_pc", null, !alreadyOpen))
                                {
                                    gui.OpenDocument(map.FileName, map.FileName, new MapEditorDocument(map.FileName));
                                }
                            }
                            ImGui.EndMenu();
                        }
                        if (ImGui.BeginMenu("Wrecking Crew"))
                        {
                            ScopedLock!(_mapListLock);
                            for (MapOption map in _wcMaps)
                            {
                                bool alreadyOpen = gui.Documents.Any((doc) => doc.Title == map.FileName);
                                if (ImGui.MenuItem(map.DisplayName, scope $"{map.FileName}.vpp_pc", null, !alreadyOpen))
                                {
                                    gui.OpenDocument(map.FileName, map.FileName, new MapEditorDocument(map.FileName));
                                }
                            }
                            ImGui.EndMenu();
                        }
                        ImGui.EndMenu();
                    }
                }
                else
                {
                    if (ImGui.BeginMenu("Open a project to edit maps", false))
                    {
                        ImGui.EndMenu();
                    }
                }

                /*if (ImGui.BeginMenu("Help"))
                {
                    if (ImGui.MenuItem("Welcome")) { }
                    if (ImGui.MenuItem("Metrics")) { }
                    if (ImGui.MenuItem("About")) { }
                    ImGui.EndMenu();
                }*/

                var drawList = ImGui.GetWindowDrawList();
                ImGui.Vec2 cursorPos = .(ImGui.GetCursorPosX(), mainMenuContentStartY);

                //Project name
                String projectName = scope .()..AppendF("|    {}", NanoDB.CurrentProject.Loaded ? NanoDB.CurrentProject.Name : "No project loaded");
                drawList.AddText(cursorPos, 0xF2F5FAFF, projectName.CStr(), projectName.CStr() + projectName.Length);

                cursorPos.x += ImGui.CalcTextSize(projectName.CStr()).x;

                //Frametime meter
                String realFrameTime = scope String()..AppendF("{0:G3}", frameData.AverageFrameTime * 1000.0f);
                String totalFrameTime = scope String()..AppendF("/  {0:G4}", frameData.DeltaTime * 1000.0f);
                char8* frametimeLabel = "     |    Frametime (ms): ";
                drawList.AddText(cursorPos, 0xF2F5FAFF, frametimeLabel);
                var textSize = ImGui.CalcTextSize(frametimeLabel);
                cursorPos.x += textSize.x + 5.0f;
                drawList.AddText(cursorPos, ImGui.ColorConvertFloat4ToU32(ImGui.SecondaryTextColor), realFrameTime.CStr());
                cursorPos.x += 42.0f;
                drawList.AddText(cursorPos, ImGui.ColorConvertFloat4ToU32(ImGui.SecondaryTextColor), totalFrameTime.CStr());

                //Draw NF version on the right side
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
                //ImGui.ID dockLeftId = ImGui.DockBuilderSplitNode(DockspaceId, .Left, 0.22f, var outIdLeft, out DockspaceId);
                ImGui.ID dockRightId = ImGui.DockBuilderSplitNode(DockspaceId, .Right, 0.22f, var outIdRight, out DockspaceId);
                ImGui.ID dockLeftBottomId = ImGui.DockBuilderSplitNode(dockRightId, .Down, 0.58f, var outIdBottom, out dockRightId);
                //ImGui.ID dockRightId = ImGui.DockBuilderSplitNode(DockspaceId, .Right, 0.28f, var outIdRight, out DockspaceId);
                //ImGui.ID dockRightUp = ImGui.DockBuilderSplitNode(dockRightId, .Up, 0.35f, var outIdRightUp, out dockRightId);
                DockspaceCentralNodeId = ImGui.DockBuilderGetCentralNode(DockspaceId).ID;
                ImGui.ID dockCentralDownSplitId = ImGui.DockBuilderSplitNode(DockspaceCentralNodeId, .Down, 0.20f, var outIdCentralDown, out DockspaceCentralNodeId);

                //Todo: Tie panel titles to these calls so both copies of the strings don't need to be updated every time the code changes
                //ImGui.DockBuilderDockWindow("Start page", DockspaceCentralNodeId);
                //ImGui.DockBuilderDockWindow("File explorer", dockLeftId);
                ImGui.DockBuilderDockWindow("Dear ImGui Demo", dockRightId);
                ImGui.DockBuilderDockWindow(StateViewer.ID.Ptr, dockRightId);
                ImGui.DockBuilderDockWindow(gui.OutlinerIdentifier, dockRightId);
                ImGui.DockBuilderDockWindow(gui.InspectorIdentifier, dockLeftBottomId);
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

        private void LoadMPMapsFromXtbl(StringView xtblPath)
        {
            switch (PackfileVFS.ReadAllText(xtblPath))
            {
                case .Ok(String text):
                    defer delete text;
                    Xml xml = scope .();
                    xml.LoadFromString(text, (int32)text.Length);
                    XmlNode root = xml.ChildNodes[0];
                    XmlNode table = root.Find("Table");
                    ScopedLock!(_mapListLock);

                    XmlNodeList mpLevelNodes = table.FindNodes("mp_level_list");
                    defer delete mpLevelNodes;
                    for (XmlNode levelNode in mpLevelNodes)
                    {
                        XmlNode nameNode = levelNode.Find("Name");
                        XmlNode fileNameNode = levelNode.Find("Filename");
                        _mpMaps.Add(new MapOption(nameNode.NodeValue, fileNameNode.NodeValue));
                    }

                    //Sort alphabetically
                    _mpMaps.Sort(scope (a, b) => String.Compare(a.DisplayName, b.DisplayName, false));

                case .Err:
                    Logger.Error("Failed to load {0} for MP maps list.", xtblPath);
                    return;
            }
        }

        private void LoadWCMapsFromXtbl(StringView xtblPath)
        {
            switch (PackfileVFS.ReadAllText(xtblPath))
            {
                case .Ok(String text):
                    defer delete text;
                    Xml xml = scope .();
                    xml.LoadFromString(text, (int32)text.Length);
                    XmlNode root = xml.ChildNodes[0];
                    XmlNode table = root.Find("Table");
                    XmlNode entry = table.Find("entry");
                    XmlNode maps = entry.Find("maps");
                    ScopedLock!(_mapListLock);

                    XmlNodeList mapNodes = maps.FindNodes("map");
                    defer delete mapNodes;
                    for (XmlNode mapNode in mapNodes)
                    {
                        XmlNode nameNode = mapNode.Find("file_name"); //TODO: Use display_name and localize once localization support is added
                        XmlNode fileNameNode = mapNode.Find("file_name");
                        _wcMaps.Add(new MapOption(nameNode.NodeValue, fileNameNode.NodeValue));
                    }

                    //TODO: Enable and fix for map names using numbers (e.g. 1.vpp_pc, 2.vpp_pc, etc in Terraform patch).
					//      The alphabetical search puts them in the wrong order. Probably want to check if the string is numeric or starts with numbers and have special logic for those ones.
                    //      Issue: https://github.com/rfg-modding/Nanoforge/issues/145
                    //Sort alphabetically
                    //_wcMaps.Sort(scope (a, b) => String.Compare(a.DisplayName, b.DisplayName, false));

                case .Err:
                    Logger.Error("Failed to load {0} for MP maps list.", xtblPath);
                    return;
            }
        }

        //Run when the data folder changed and PackfileVFS finished mounting that directory
        private void OnDataFolderChanged()
        {
            Logger.Info("Data folder changed. Reloading map list for main menu bar...");

            //Clear previous load
            if (_loadedMapLists)
            {
                ScopedLock!(_mapListLock);
                ClearAndDeleteItems(_mpMaps);
                ClearAndDeleteItems(_wcMaps);
                _loadedMapLists = false;
            }

            //Load maps list
            LoadMPMapsFromXtbl("//data/misc.vpp_pc/mp_levels.xtbl");
            LoadMPMapsFromXtbl("//data/misc.vpp_pc/dlc02_mp_levels.xtbl");
            LoadWCMapsFromXtbl("//data/misc.vpp_pc/wrecking_crew.xtbl");
            LoadWCMapsFromXtbl("//data/misc.vpp_pc/dlc03_wrecking_crew.xtbl");
            _loadedMapLists = true;
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