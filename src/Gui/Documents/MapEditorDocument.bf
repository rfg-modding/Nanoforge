#pragma warning disable 168
using System.Threading;
using Nanoforge.App;
using Nanoforge.Rfg;
using Common;
using System;
using ImGui;
using Nanoforge.Rfg.Import;
using Nanoforge.Misc;
using Nanoforge.Render;
using Common.Math;
using Nanoforge.Render.Resources;
using System.Collections;
using System.Diagnostics;
using Nanoforge.Gui.Documents.MapEditor;
using System.Reflection;
using Bon;
using NativeFileDialog;

namespace Nanoforge.Gui.Documents
{
	public class MapEditorDocument : GuiDocumentBase
	{
        public static Stopwatch TotalImportTimer = new Stopwatch(false) ~delete _;
        public static Stopwatch TotalImportAndLoadTimer = new Stopwatch(false) ~delete _;

        public readonly append String MapName;
        public bool Loading { get; private set; } = true;
        private bool _loadFailure = false;
        private StringView _loadFailureReason = .();
        public Territory Map = null;
        private Renderer _renderer = null;
        private Scene _scene = null;
        private ZoneObject _selectedObject = null;
        private ZoneObject SelectedObject
        {
            get => _selectedObject;
            set
            {
                _selectedObject = value;
                for (Zone zone in Map.Zones)
                {
                    for (ZoneObject obj in zone.Objects)
                    {
                        if (obj == value)
                        {
                            _selectedObjectZone = zone;
                            return;
                        }
                    }
                }
                _selectedObjectZone = null;
            }
        }
        private Zone _selectedObjectZone = null; //The zone that the selected object lies in
        private Zone _selectedZone = null;

        //Deepest tree level the outliner will draw. Need to limit to prevent accidental stack overflow
        private const int MAX_OUTLINER_DEPTH = 7;

        private append Dictionary<Type, Type> _inspectorTypes;
        //private append List<ZoneObjectClass> _objectClasses ~ClearAndDeleteItems(_);
        public static append CVar<ZoneObjectClasses> CVar_ObjectClasses = .("Object Classes");
        //Returned by GetObjectClass() when it can't find it
        private append ZoneObjectClass _unknownObjectClass = .("unknown", .(1.0f, 1.0f, 1.0f), true, Icons.ICON_FA_QUESTION);

        //TODO: Make these into cvars once they're added
        private bool _onlyShowPersistentObjects = false;
        //private bool _highlightHoveredZone = false; //TODO: Implement once multi zone maps are supported again
        //private float _zoneBoxHeight = 150.0f;
        private bool _highlightHoveredObject = false;
        private bool _autoMoveChildren = true; //Auto move children when the parent object is moved

        [BonTarget]
        public class MapEditorSettings : EditorObject
        {
            public String MapExportPath = new .() ~delete _;
        }

        public static append CVar<MapEditorSettings> CVar_MapEditorSettings = .("Map Editor Settings");

        private bool _showMapExportFolderSelectorError = false;
        private append String _mapExportFolderSelectorError;

        public this(StringView mapName)
        {
            SetupObjectClasses();
            MapName.Set(mapName);
            HasMenuBar = false;
            NoWindowPadding = true;
            HasCustomOutlinerAndInspector = true;

            //Find inspectors
            for (Type inspectorType in Type.Types)
            {
                if (inspectorType.GetCustomAttribute<RegisterInspectorAttribute>() case .Ok(RegisterInspectorAttribute attribute))
                {
                    Type zoneObjectType = attribute.ZoneObjectType;
                    if (_inspectorTypes.ContainsKey(zoneObjectType))
                    {
                        //More than one inspectors have been registered for one type
                        Logger.Error(scope $"More than one inspector has been registered for type '{zoneObjectType.GetName(.. scope .())}'");
                    }
                    else
                    {
                        _inspectorTypes.Add(zoneObjectType, inspectorType);
                    }
                }
            }
        }

        public ~this()
        {
            if (_scene != null)
            {
                _renderer.DestroyScene(_scene);
                _scene = null;
            }
        }

        private void Load(App app)
        {
            Loading = true;
            TotalImportAndLoadTimer.Start();
            defer { Loading = false; TotalImportAndLoadTimer.Stop(); }

            Logger.Info("Opening {}...", MapName);
            Stopwatch loadTimer = scope .(true);

            //Create scene for rendering
            Renderer renderer = app.GetResource<Renderer>();
            defer { _scene.Active = true; }

            //Check if the map was already imported
            Territory findResult = NanoDB.Find<Territory>(MapName);
            if (findResult == null)
            {
                //Map needs to be imported
                Logger.Info("First time opening {} in this project. Importing...", MapName);
                Stopwatch importTimer = scope .(true);
                TotalImportTimer.Start();
                switch (MapImporter.ImportMap(MapName))
                {
                    case .Ok(var newMap):
                		Map = newMap;
                        Logger.Info("Finished importing map {} in {}s", MapName, importTimer.Elapsed.TotalSeconds);
                    case .Err(StringView err):
                		_loadFailure = true;
                        _loadFailureReason = err;
                        Logger.Error("Failed to import map {}. {}. See the log for more details.", MapName, err);
                        return;
                }
                TotalImportTimer.Stop();

                UnsavedChanges = true;
            }
            else
            {
                Logger.Info("{} import found in ProjectDB.", MapName);
                Map = findResult;
            }

            //Import complete. Now load
            if (Map.Load(renderer, _scene) case .Err(StringView err))
            {
                Logger.Error("An error occurred while loading {}. Halting loading. Error: {}", MapName, err);
                _loadFailure = true;
                _loadFailureReason = err;
                return;
            }

            //Temporary hardcoded settings for high lod terrain rendering. Will be removed once config gui is added
            _scene.PerFrameConstants.ShadeMode = 1;
            _scene.PerFrameConstants.DiffuseIntensity = 1.2f;
            _scene.PerFrameConstants.DiffuseColor = Colors.RGBA.White;

            //Auto center camera on zone closest to the map origin
            Vec3 closestZonePos = .(312.615f, 56.846f, -471.078f);
            f32 closestDistanceFromOrigin = f32.MaxValue;
            for (Zone zone in Map.Zones)
            {
                ObjZone objZone = null;
                for (ZoneObject obj in zone.Objects)
                {
                    if (obj.GetType() == typeof(ObjZone))
                    {
                        objZone = (ObjZone)obj;
                        break;
                    }
                }

                if (objZone != null)
                {
                    f32 distance = objZone.Position.Length;
                    if (distance < closestDistanceFromOrigin)
                    {
                        closestDistanceFromOrigin = distance;
                        closestZonePos = objZone.Position;
                    }
                }
            }
            _scene.Camera.TargetPosition = closestZonePos;
            _scene.Camera.TargetPosition.x += 125.0f;
            _scene.Camera.TargetPosition.y = 250.0f;
            _scene.Camera.TargetPosition.z -= 250.0f;

            CountObjectClassInstances();
            Logger.Info("{} loaded in {}s", MapName, loadTimer.Elapsed.TotalSeconds);
        }

        public override void Update(App app, Gui gui)
        {
            //Focus the window when right clicking it. Otherwise had to left click the window first after selecting another to control the camera again. Was annoying
            if (ImGui.IsMouseDown(.Right) && ImGui.IsWindowHovered())
            {
                ImGui.SetWindowFocus();
            }

            if (FirstDraw)
            {
                //Start loading process on first draw. We create the scene here to avoid needing to synchronize D3D11DeviceContext usage between multiple threads
                _renderer = app.GetResource<Renderer>();
                _scene = _renderer.CreateScene(false);
                ThreadPool.QueueUserWorkItem(new () => { this.Load(app); });
            }
            if (Loading || _loadFailure)
	            return;

            if (_scene != null)
            {
                //Camera should only handle input when the viewport is focused or mouse hovered. Otherwise it'll react while you're typing into the inspector/outliner
                _scene.Camera.InputEnabled = ImGui.IsWindowFocused() && ImGui.IsWindowHovered();

                //Update scene viewport size
                ImGui.Vec2 contentAreaSize;
                contentAreaSize.x = ImGui.GetWindowContentRegionMax().x - ImGui.GetWindowContentRegionMin().x;
                contentAreaSize.y = ImGui.GetWindowContentRegionMax().y - ImGui.GetWindowContentRegionMin().y;
                if (contentAreaSize.x > 0.0f && contentAreaSize.y > 0.0f)
                {
                    _scene.Resize((u32)contentAreaSize.x, (u32)contentAreaSize.y);
                }

                //Store initial position so we can draw buttons over the scene texture after drawing it
                ImGui.Vec2 initialPos = ImGui.GetCursorPos();

                //Render scene texture
                ImGui.PushStyleColor(.WindowBg, .(_scene.ClearColor.x, _scene.ClearColor.y, _scene.ClearColor.z, _scene.ClearColor.w));
                ImGui.Image(_scene.View, .(_scene.ViewWidth, _scene.ViewHeight));
                ImGui.PopStyleColor();

                //Set cursor pos to top left corner to draw buttons over scene texture
                ImGui.Vec2 adjustedPos = initialPos;
                adjustedPos.x += 10.0f;
                adjustedPos.y += 10.0f;
                ImGui.SetCursorPos(adjustedPos);

                DrawMenuBar(app, gui);
            }

            //Don't redraw if this document isn't focused by the user. 
            _scene.Active = (this == gui.FocusedDocument);
            if (!_scene.Active)
                return;

            //Draw object bounding boxes
            for (Zone zone in Map.Zones)
            {
                for (ZoneObject obj in zone.Objects)
                {
                    ZoneObjectClass objectClass = GetObjectClass(obj);
                    if (!objectClass.Visible)
                        continue;

                    Vec4 color = .(objectClass.Color.x, objectClass.Color.y, objectClass.Color.z, 1.0f);
                    _scene.DrawBox(obj.BBox.Min, obj.BBox.Max, color);
                }
            }
        }

        private void DrawMenuBar(App app, Gui gui)
        {
            ImGui.SetNextItemWidth(_scene.ViewWidth);
            ImGui.PushStyleVar(.WindowPadding, .(8.0f, 8.0f)); //Must manually set padding here since the parent window has padding disabled to get the viewport flush with the window border.
            bool openScenePopup = false;
            bool openCameraPopup = false;
            bool openExportPopup = false;

            Fonts.FontXL.Push();
            if (ImGui.Button(Icons.ICON_FA_FILE_EXPORT))
            {
                openExportPopup = true;
            }
            Fonts.FontXL.Pop();

            //NOTE: I left in the C++ versions of these to be ported later. It won't be very long until then.
            //      Also, for some reason this main menu bar won't draw in the rewrite despite the code being identical aside from the disabled menu options. For now a button works but I'll try fixing it again when I implement the rest of the menu bar.

            /*if (ImGui.BeginMenuBar())
            {

                /*if (ImGui.BeginMenu("View"))
                {
                    if (ImGui.MenuItem("Scene"))
                        openScenePopup = true;
                    if (ImGui.MenuItem("Camera"))
                        openCameraPopup = true;

                    ImGui.EndMenu();
                }
                if (ImGui.BeginMenu("Object"))
                {
                    bool canClone = selectedObject_.Valid();
                    bool canDelete = selectedObject_.Valid();
                    bool canRemoveWorldAnchors = selectedObject_.Valid() && selectedObject_.Has("world_anchors");
                    bool canRemoveDynamicLinks = selectedObject_.Valid() && selectedObject_.Has("dynamic_links");
                    bool hasChildren = selectedObject_.Valid() && selectedObject_.Has("Children") && selectedObject_.GetObjectList("Children").size() > 0;
                    bool hasParent = selectedObject_.Valid() && selectedObject_.Get<ObjectHandle>("Parent").Valid();

                    if (ImGui.MenuItem("Clone", "Ctrl + D", null, canClone))
                    {
                        ShallowCloneObject(selectedObject_);
                        _scrollToObjectInOutliner = selectedObject_;
                    }
                    if (ImGui.MenuItem("Deep clone", "", null, canClone))
                    {
                        DeepCloneObject(selectedObject_, true);
                        _scrollToObjectInOutliner = selectedObject_;
                    }

                    ImGui.Separator();
                    if (ImGui.MenuItem("Orphan object", null, null, hasParent))
                    {
                        orphanObjectPopupHandle_ = selectedObject_;
                        orphanObjectPopup_.Open();
                    }
                    if (ImGui.MenuItem("Orphan children", null, null, hasChildren))
                    {
                        orphanChildrenPopupHandle_ = selectedObject_;
                        orphanChildrenPopup_.Open();
                    }
                    if (ImGui.MenuItem("Add dummy object", null))
                    {
                        AddGenericObject();
                        _scrollToObjectInOutliner = selectedObject_;
                    }
                    ImGui.SameLine();
                    gui::HelpMarker("Creates a dummy object that you can turn into any object type. Improperly configured objects can crash the game, so use at your own risk.", null);

                    ImGui.Separator();
                    if (ImGui.MenuItem("Delete", "Delete", null, canDelete))
                    {
                        DeleteObject(selectedObject_);
                    }

                    ImGui.Separator();
                    if (ImGui.MenuItem("Copy scriptx reference", "Ctrl + I", null, canDelete))
                    {
                        CopyScriptxReference(selectedObject_);
                    }
                    if (ImGui.MenuItem("Remove world anchors", "Ctrl + B", null, canRemoveWorldAnchors))
                    {
                        RemoveWorldAnchors(selectedObject_);
                    }
                    if (ImGui.MenuItem("Remove dynamic links", "Ctrl + N", null, canRemoveDynamicLinks))
                    {
                        RemoveDynamicLinks(selectedObject_);
                    }

                    ImGui.EndMenu();
                }*/
                if (ImGui.MenuItem("Export"))
                {
                    openExportPopup = true;
                }

                ImGui.EndMenuBar();
            }*/

            //Have to open the popup in the same scope as BeginPopup(), can't do it in the menu item result. Annoying restriction for imgui popups.
            /*if (openScenePopup)
                ImGui.OpenPopup("##ScenePopup");
            if (openCameraPopup)
                ImGui.OpenPopup("##CameraPopup");*/
            if (openExportPopup)
                ImGui.OpenPopup("##MapExportPopup");

            //DrawObjectCreationPopup(state);

            //Scene settings popup
            /*if (ImGui.BeginPopup("##ScenePopup"))
            {
                state->FontManager->FontL.Push();
                ImGui.Text(ICON_FA_SUN " Scene settings");
                state->FontManager->FontL.Pop();

                //If popup is visible then redraw scene each frame. Simpler than trying to add checks for each option changing
                Scene->NeedsRedraw = true;

                ImGui.Text("Shading mode: ");
                ImGui.SameLine();
                ImGui.RadioButton("Elevation", &Scene->perFrameStagingBuffer_.ShadeMode, 0);
                ImGui.SameLine();
                ImGui.RadioButton("Diffuse", &Scene->perFrameStagingBuffer_.ShadeMode, 1);

                if (Scene->perFrameStagingBuffer_.ShadeMode != 0)
                {
                    ImGui.Text("Diffuse presets: ");
                    ImGui.SameLine();
                    if (ImGui.Button("Default"))
                    {
                        Scene->perFrameStagingBuffer_.DiffuseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
                        CVar_DiffuseIntensity.Get<f32>() = 1.2f;
                        Config::Get()->Save();
                        Scene->perFrameStagingBuffer_.ElevationFactorBias = 0.8f;
                    }

                    ImGui.ColorEdit3("Diffuse", reinterpret_cast<f32*>(&Scene->perFrameStagingBuffer_.DiffuseColor));
                    if (ImGui.SliderFloat("Diffuse intensity", &CVar_DiffuseIntensity.Get<f32>(), 0.0f, 2.0f))
                    {
                        Config::Get()->Save();
                    }
                }

                if(ImGui.SliderFloat("Zone object distance", &CVar_ZoneObjectDistance.Get<f32>(), 0.0f, 10000.0f))
                {
                    Config::Get()->Save();
                }
                ImGui.SameLine();
                gui::HelpMarker("Zone object bounding boxes and meshes aren't drawn beyond this distance from the camera.", ImGui.GetIO().FontDefault);

                if (useHighLodTerrain_)
                {
                    if (ImGui.Checkbox("High lod terrain enabled", &highLodTerrainEnabled_))
                    {
                        terrainVisiblityUpdateNeeded_ = true;
                    }
                    if (highLodTerrainEnabled_)
                    {
                        if (ImGui.SliderFloat("High lod distance", &CVar_HighLodTerrainDistance.Get<f32>(), 0.0f, 10000.0f))
                        {

                        }
                        ImGui.SameLine();
                        gui::HelpMarker("Beyond this distance from the camera low lod terrain is used.", ImGui.GetIO().FontDefault);
                        terrainVisiblityUpdateNeeded_ = true;
                    }
                }

                bool drawChunkMeshes = CVar_DrawChunkMeshes.Get<bool>();
                if (ImGui.Checkbox("Show buildings", &drawChunkMeshes))
                {
                    CVar_DrawChunkMeshes.Get<bool>() = drawChunkMeshes;
                    Config::Get()->Save();
                }
                if (drawChunkMeshes)
                {
                    if (ImGui.SliderFloat("Building distance", &CVar_BuildingDistance.Get<f32>(), 0.0f, 10000.0f))
                    {
                        Config::Get()->Save();
                    }
                    ImGui.SameLine();
                    gui::HelpMarker("Beyond this distance from the camera buildings won't be drawn.", ImGui.GetIO().FontDefault);
                    terrainVisiblityUpdateNeeded_ = true;
                }

                ImGui.EndPopup();
            }*/

            //Camera settings popup
            /*if (ImGui.BeginPopup("##CameraPopup"))
            {
                state->FontManager->FontL.Push();
                ImGui.Text(ICON_FA_CAMERA " Camera");
                state->FontManager->FontL.Pop();

                //Sync cvar with camera speed in case it was changed with the scrollbar
                if (CVar_CameraSpeed.Get<f32>() != Scene->Cam.Speed)
                {
                    CVar_CameraSpeed.Get<f32>() = Scene->Cam.Speed;
                    Config::Get()->Save();
                }

                //If popup is visible then redraw scene each frame. Simpler than trying to add checks for each option changing
                Scene->NeedsRedraw = true;

                f32 fov = Scene->Cam.GetFovDegrees();
                f32 nearPlane = Scene->Cam.GetNearPlane();
                f32 farPlane = Scene->Cam.GetFarPlane();
                f32 lookSensitivity = Scene->Cam.GetLookSensitivity();

                if (ImGui.Button("0.1")) CVar_CameraSpeed.Get<f32>() = 0.1f;
                ImGui.SameLine();
                if (ImGui.Button("1.0")) CVar_CameraSpeed.Get<f32>() = 1.0f;
                ImGui.SameLine();
                if (ImGui.Button("10.0")) CVar_CameraSpeed.Get<f32>() = 10.0f;
                ImGui.SameLine();
                if (ImGui.Button("25.0")) CVar_CameraSpeed.Get<f32>() = 25.0f;
                ImGui.SameLine();
                if (ImGui.Button("50.0")) CVar_CameraSpeed.Get<f32>() = 50.0f;
                ImGui.SameLine();
                if (ImGui.Button("100.0")) CVar_CameraSpeed.Get<f32>() = 100.0f;

                if (ImGui.InputFloat("Speed", &CVar_CameraSpeed.Get<f32>()))
                {
                    Config::Get()->Save();
                }
                ImGui.InputFloat("Sprint speed", &Scene->Cam.SprintSpeed);

                if (ImGui.SliderFloat("Fov", &fov, 40.0f, 120.0f))
                    Scene->Cam.SetFovDegrees(fov);
                if (ImGui.InputFloat("Near plane", &nearPlane))
                    Scene->Cam.SetNearPlane(nearPlane);
                if (ImGui.InputFloat("Far plane", &farPlane))
                    Scene->Cam.SetFarPlane(farPlane);
                if (ImGui.InputFloat("Look sensitivity", &lookSensitivity))
                    Scene->Cam.SetLookSensitivity(lookSensitivity);

                if (ImGui.InputFloat3("Position", (float*)&Scene->Cam.camPosition))
                {
                    Scene->Cam.UpdateViewMatrix();
                }

                //Sync camera speed with cvar
                if (Scene->Cam.Speed != CVar_CameraSpeed.Get<f32>())
                {
                    Scene->Cam.Speed = CVar_CameraSpeed.Get<f32>();
                    Config::Get()->Save();
                }

                gui::LabelAndValue("Pitch:", std::to_string(Scene->Cam.GetPitchDegrees()));
                gui::LabelAndValue("Yaw:", std::to_string(Scene->Cam.GetYawDegrees()));
                if (ImGui.Button("Zero"))
                {
                    Scene->Cam.yawRadians_ = ToRadians(180.0f);
                    Scene->Cam.UpdateProjectionMatrix();
                    Scene->Cam.UpdateViewMatrix();
                }
                ImGui.EndPopup();
            }*/

            //Map export popup
            if (ImGui.BeginPopup("##MapExportPopup"))
            {
                Fonts.FontL.Push();
                ImGui.Text(scope $"{StringView(Icons.ICON_FA_MOUNTAIN)} Export map");
                Fonts.FontL.Pop();
                ImGui.Separator();

                if (Loading)
                    ImGui.TextColored(.(1.0f, 0.0f, 0.0f, 1.0f), "Still loading map...");
                else if (_loadFailure)
                    ImGui.TextColored(.(1.0f, 0.0f, 0.0f, 1.0f), "Map data failed to load.");
                else if (gTaskDialog.Open)
                    ImGui.TextColored(.(1.0f, 0.0f, 0.0f, 1.0f), "Export in progress. Please wait...");
                else
                {
                    //Select export folder path
                    String exportPath = CVar_MapEditorSettings->MapExportPath;
                    String initialPath = scope String()..Append(exportPath);
                    ImGui.InputText("Export path", exportPath);
                    ImGui.SameLine();
                    if (ImGui.Button("..."))
                    {
                        char8* outPath = null;
                        switch (NativeFileDialog.PickFolder(exportPath.CStr(), &outPath))
                        {
                            case .Okay:
                                exportPath.Set(StringView(outPath));
                                _mapExportFolderSelectorError.Set("");
                            case .Error:
                                char8* error = NativeFileDialog.GetError();
                                Logger.Error("Error opening folder selector in map export dialog: {}", StringView(error));
                                _mapExportFolderSelectorError.Set(StringView(error));
                                _showMapExportFolderSelectorError = true;
                            default:
                                _mapExportFolderSelectorError.Set("");
                                break;
                        }
                    }
                    if (initialPath != exportPath)
                        CVar_MapEditorSettings.Save();

                    if (_showMapExportFolderSelectorError)
                    {
                        ImGui.TextColored(.Red, _mapExportFolderSelectorError);
                    }    

                    if (ImGui.Button("Export"))
                    {

                        ImGui.CloseCurrentPopup();
                    }
                }

                ImGui.EndPopup();
            }

            ImGui.PopStyleVar();
        }

        public override void Save(App app, Gui gui)
        {

        }

        public override void OnClose(App app, Gui gui)
        {

        }

        public override bool CanClose(App app, Gui gui)
        {
            return true;
        }

        public override void Outliner(App app, Gui gui)
        {
            if (Loading)
            {
                ImGui.Text("Map is loading...");
                return;
            }
            else if (_loadFailure)
            {
                ImGui.Text(scope $"Load error! {_loadFailureReason}");
                return;
            }

            Outliner_DrawFilters();

            //Set custom highlight colors for the table
            Vec4 selectedColor = .(0.157f, 0.350f, 0.588f, 1.0f);
            Vec4 highlightedColor = selectedColor * 1.1f;
            ImGui.PushStyleColor(.Header, selectedColor);
            ImGui.PushStyleColor(.HeaderHovered, highlightedColor);
            ImGui.PushStyleColor(.HeaderActive, highlightedColor);
            ImGui.TableFlags tableFlags = .ScrollY | .ScrollX | .RowBg | .BordersOuter | .BordersV | .Resizable | .Reorderable | .Hideable | .SizingFixedFit;
            if (ImGui.BeginTable("ZoneObjectTable", 4, tableFlags))
            {
                ImGui.TableSetupScrollFreeze(0, 1);
                ImGui.TableSetupColumn("Name");
                ImGui.TableSetupColumn("Flags");
                ImGui.TableSetupColumn("Num");
                ImGui.TableSetupColumn("Handle");
                ImGui.TableHeadersRow();

                //Loop through visible zones
                bool anyZoneHovered = false;
                for (Zone zone in Map.Zones)
                {
                    ImGui.TableNextRow();
                    ImGui.TableNextColumn();

                    bool anyChildrenVisible = Outliner_ZoneAnyChildObjectsVisible(zone);
                    if (!anyChildrenVisible)
                        continue;

                    bool selected = (zone == _selectedZone);
                    ImGui.TreeNodeFlags nodeFlags = .SpanAvailWidth | (anyChildrenVisible ? .None : .Leaf);
                    if (selected)
                        nodeFlags |= .Selected;

                    //Draw tree node for zones if there's > 1. Otherwise draw the objects at the root of the tree
                    bool singleZone = Map.Zones.Count == 1;
                    bool treeNodeOpen = false;
                    if (!singleZone)
                    {
						treeNodeOpen = ImGui.TreeNodeEx(zone.Name.CStr(), nodeFlags);
                    }

                    if (singleZone || treeNodeOpen)
                    {
                        for (ZoneObject obj in zone.Objects)
                        {
                            //Don't draw objects with parents at the top of the tree. They'll be drawn as subnodes of their parent
                            if (obj.Parent != null)
                                continue;

                            int depth = 0;
                            Outliner_DrawObjectNode(obj, depth + 1);
                        }
                    }

                    if (treeNodeOpen)
	                    ImGui.TreePop();
                }

                ImGui.EndTable();
            }

            ImGui.PopStyleColor(3);
        }

        private void Outliner_DrawFilters()
        {
            if (ImGui.Button(scope String(Icons.ICON_FA_FILTER)..Append(" Filters")..EnsureNullTerminator()))
                ImGui.OpenPopup("##ObjectFilterPopup");

            bool filtersChanged = false;
            if (ImGui.BeginPopup("##ObjectFilterPopup"))
            {
                if (ImGui.Button("Show all types"))
                {
                    filtersChanged = true;
                    for (ZoneObjectClass objectClass in CVar_ObjectClasses->Classes)
                    {
                        objectClass.Visible = true;
                    }
                }
                ImGui.SameLine();
                if (ImGui.Button("Hide all types"))
                {
                    filtersChanged = true;
                    for (ZoneObjectClass objectClass in CVar_ObjectClasses->Classes)
                    {
                        objectClass.Visible = false;
                    }
                }
                
                if (ImGui.Checkbox("Only show persistent", &_onlyShowPersistentObjects))
                {
                    filtersChanged = true;
                }
                ImGui.SameLine();
                ImGui.HelpMarker("Only draw objects with the persistent flag checked");

                //Note: Included in the port. Disabled until multi-zone maps are supported again. At the moment it only opens MP/WC maps, which are single zone.
                //When checked the zone being moused over in the outliner has a solid box draw on it in the editor. Also toggleable with the F key
                /*ImGui.Checkbox("Highlight zones", &_highlightHoveredZone);
                ImGui.SameLine();
                ImGui.SetNextItemWidth(100.0f);
                ImGui.InputFloat("Height", &_zoneBoxHeight);
                ImGui.SameLine();
                ImGui.HelpMarker("Draw a solid box over zones when they're moused over in the outliner. Can also be toggled with the F key");*/

                //When checked draw solid bbox over moused over objects
                ImGui.Checkbox("Highlight objects", &_highlightHoveredObject);
                ImGui.SameLine();
                ImGui.HelpMarker("Draw a solid box over objects when they're moused over in the outliner. Can also be toggled with the G key");

                ImGui.Checkbox("Auto move children", &_autoMoveChildren);
                ImGui.SameLine();
                ImGui.HelpMarker("Automatically move child objects by the same amount when moving their parents");

                //Set custom highlight colors for the table. It looks nicer this way.
                ImGui.Vec4 selectedColor = .(0.157f, 0.350f, 0.588f, 1.0f);
                ImGui.Vec4 highlightColor = .(selectedColor.x * 1.1f, selectedColor.y * 1.1f, selectedColor.z * 1.1f, 1.0f);
                ImGui.PushStyleColor(.Header, selectedColor);
                ImGui.PushStyleColor(.HeaderHovered, highlightColor);
                ImGui.PushStyleColor(.HeaderActive, highlightColor);

                //Draw object filters table. One row per object type
                ImGui.TableFlags flags = .ScrollY | .ScrollX | .RowBg | .BordersOuter | .BordersV | .Resizable | .Reorderable | .Hideable | .SizingFixedFit;
                if (ImGui.BeginTable("ObjectFilterTable", 5, flags, .(450.0f, 500.0f)))
                {
                    ImGui.TableSetupScrollFreeze(0, 1); //Freeze header row so it's always visible
                    ImGui.TableSetupColumn("Type");
                    ImGui.TableSetupColumn("Visible");
                    ImGui.TableSetupColumn("Solid");
                    ImGui.TableSetupColumn("Count");
                    ImGui.TableSetupColumn("Color");
                    ImGui.TableHeadersRow();

                    for (ZoneObjectClass objectClass in CVar_ObjectClasses->Classes)
                    {
                        ImGui.TableNextRow();

                        //Type
                        ImGui.TableNextColumn();
                        ImGui.Text(objectClass.Classname);

                        //Visible
                        ImGui.TableNextColumn();
                        if (ImGui.Checkbox(scope $"##Visible{objectClass.Classname}", &objectClass.Visible))
                        {
                            filtersChanged = true;
                        }

                        //Solid
                        ImGui.TableNextColumn();
                        if (ImGui.Checkbox(scope $"##Solid{objectClass.Classname}", &objectClass.DrawSolid))
                        {
                            filtersChanged = true;
                        }

                        //# of objects of this type
                        ImGui.TableNextColumn();
                        ImGui.Text(scope $"{objectClass.NumInstances} objects");

                        //Object color
                        ImGui.TableNextColumn();
                        if (ImGui.ColorEdit3(scope $"##Color{objectClass.Classname}", ref *(float[3]*)&objectClass.Color, .NoInputs | .NoLabel))
                        {
                            filtersChanged = true;
                        }
                    }

                    ImGui.EndTable();
                }
                ImGui.PopStyleColor(3);

                ImGui.EndPopup();
            }

            if (filtersChanged)
            {
                CVar_ObjectClasses.Save();
            }
        }

        private bool Outliner_ZoneAnyChildObjectsVisible(Zone zone)
        {
            for (ZoneObject obj in zone.Objects)
                if (Outliner_ShowObjectOrChildren(obj))
	                return true;

            return false;
        }

        private bool Outliner_ShowObjectOrChildren(ZoneObject obj)
        {
            ZoneObjectClass objectClass = GetObjectClass(obj);
            if (objectClass.Visible)
            {
                if (_onlyShowPersistentObjects)
                    return obj.Persistent;
                else
                    return true;
            }

            for (ZoneObject child in obj.Children)
                if (Outliner_ShowObjectOrChildren(child))
                    return true;

            return false;
        }

        private void Outliner_DrawObjectNode(ZoneObject obj, int depth)
        {
            if (depth > MAX_OUTLINER_DEPTH)
            {
                ImGui.Text("Can't draw object! Hit outliner depth limit");
                return;
            }

            bool showObjectOrChildren = Outliner_ShowObjectOrChildren(obj);
            if (!showObjectOrChildren)
                return;

            bool selected = (obj == SelectedObject);
            ZoneObjectClass objectClass = GetObjectClass(obj);
            String label = scope $"      "; //Empty space for node icon. Drawn separately to give the text and icon different colors
            if (!obj.Name.IsEmpty)
                label.Append(obj.Name);
            else
                label.Append(obj.Classname);

            ImGui.TableNextRow();
            ImGui.TableNextColumn();

            bool hasChildren = obj.Children.Count > 0;
            bool hasParent = obj.Parent != null;

            //Draw node
            ImGui.PushID(Internal.UnsafeCastToPtr(obj));
            f32 nodeXPos = ImGui.GetCursorPosX(); //Store position of the node for drawing the node icon later
            bool nodeOpen = ImGui.TreeNodeEx(label.CStr(), .SpanAvailWidth | .OpenOnDoubleClick | .OpenOnArrow | (hasChildren ? .None : .Leaf) | (selected ? .Selected : .None));

            if (ImGui.IsItemClicked())
            {
                if (obj == SelectedObject)
                    SelectedObject = null;
                else
                    SelectedObject = obj;
            }
            if (ImGui.IsItemHovered())
            {
                obj.Classname.EnsureNullTerminator();
                ImGui.TooltipOnPrevious(obj.Classname);
            }

            //Draw node icon
            ImGui.PushStyleColor(.Text, .(objectClass.Color.x, objectClass.Color.y, objectClass.Color.z, 1.0f));
            ImGui.SameLine();
            ImGui.SetCursorPosX(nodeXPos + 22.0f);
            ImGui.Text(objectClass.Icon);
            ImGui.PopStyleColor();

            //Flags
            ImGui.TableNextColumn();
            ImGui.Text(obj.Flags.ToString(.. scope .())); //TODO: Make custom ToString function that includes each flag in the string instead of reverting to an integer

            //Num
            ImGui.TableNextColumn();
            ImGui.Text(obj.Num.ToString(.. scope .()));

            //Handle
            ImGui.TableNextColumn();
            ImGui.Text(obj.Handle.ToString(.. scope .()));

            //Draw child nodes
            if (nodeOpen)
            {
                for (ZoneObject child in obj.Children)
                {
                    Outliner_DrawObjectNode(child, depth + 1);
                }
                ImGui.TreePop();
            }
            ImGui.PopID();
        }

        public override void Inspector(App app, Gui gui)
        {
            if (SelectedObject == null)
                return;

            //Draw inspector for viewing and editing object properties. They're type specific classes which implement IZoneObjectInspector
            Type objectType = SelectedObject.GetType();
            if (_inspectorTypes.ContainsKey(objectType))
            {
                Type inspectorType = _inspectorTypes[objectType];
                if (inspectorType.GetMethod("Draw") case .Ok(MethodInfo methodInfo))
                {
                    methodInfo.Invoke(null, app, this, _selectedObjectZone, SelectedObject).Dispose();
                }
                else
                {
                    ImGui.Text(scope $"Failed to find draw function for '{inspectorType.GetName(.. scope .())}'");
                }
            }
            else
            {
                ImGui.Text(scope $"Unsupported zone object type '{objectType.GetName(.. scope .())}'");
            }    

            //Blank space after controls since sometimes the scrollbar doesn't go far enough down without it
            ImGui.Text("");
        }

        private void SetupObjectClasses()
        {
            //_unknownObjectClass.Init(typeof(ZoneObject), "unknown", .(1.0f, 1.0f, 1.0f), true, Icons.ICON_FA_QUESTION);
            if (CVar_ObjectClasses->Classes.Count == 0)
            {
                //Note: I included SP only classes so they just need to be uncommented when they're eventually added
                var objectClasses = CVar_ObjectClasses->Classes;
                objectClasses.Add(new .(
                	classname: "rfg_mover",
                	color: .(0.819f, 0.819f, 0.819f),
                	visible: true,
                	icon: Icons.ICON_FA_HOME));

                objectClasses.Add(new .(
                	classname: "cover_node",
                	color: .(1.0f, 0.0f, 0.0f),
                	visible: false,
                	icon: Icons.ICON_FA_SHIELD_ALT));

                objectClasses.Add(new .(
                	classname: "navpoint",
                	color: .(1.0f, 0.968f, 0.0f),
                	visible: false,
                	icon: Icons.ICON_FA_LOCATION_ARROW));

                objectClasses.Add(new .(
                	classname: "general_mover",
                	color: .(1.0f, 0.664f, 0.0f),
                	visible: true,
                	icon: Icons.ICON_FA_CUBES));

                objectClasses.Add(new .(
                	classname: "player_start",
                	color: .(0.591f, 1.0f, 0.0f),
                	visible: true,
                	icon: Icons.ICON_FA_STREET_VIEW));

                objectClasses.Add(new .(
                	classname: "multi_object_marker",
                	color: .(0.000f, 0.759f, 1.0f),
                	visible: true,
                	icon: Icons.ICON_FA_MAP_MARKER));

                objectClasses.Add(new .(
                	classname: "weapon",
                	color: .(1.0f, 0.0f, 0.0f),
                	visible: true,
                	icon: Icons.ICON_FA_CROSSHAIRS));

                objectClasses.Add(new .(
                	classname: "object_action_node",
                	color: .(0.719f, 0.494f, 0.982f),
                	visible: true,
                	icon: Icons.ICON_FA_RUNNING));

                /*_objectClasses.Add(new .(
                	classname: "object_squad_spawn_node",
                	color: .(0.719f, 0.494f, 0.982f),
                	visible: true,
                	icon: Icons.ICON_FA_USERS));*/

                /*_objectClasses.Add(new .(
                	classname: "object_npc_spawn_node",
                	color: .(0.719f, 0.494f, 0.982f),
                	visible: true,
                	icon: Icons.ICON_FA_USER));*/

                /*_objectClasses.Add(new .(
                	classname: "object_guard_node",
                	color: .(0.719f, 0.494f, 0.982f),
                	visible: true,
                	icon: Icons.ICON_FA_SHIELD_ALT));*/

                /*_objectClasses.Add(new .(
                	classname: "object_path_road",
                	color: .(0.719f, 0.494f, 0.982f),
                	visible: true,
                	icon: Icons.ICON_FA_ROAD));*/

                objectClasses.Add(new .(
                	classname: "shape_cutter",
                	color: .(1.0f, 1.0f, 1.0f),
                	visible: true,
                	icon: Icons.ICON_FA_CUT));

                objectClasses.Add(new .(
                	classname: "item",
                	color: .(0.719f, 0.494f, 0.982f),
                	visible: true,
                	icon: Icons.ICON_FA_TOOLS));

                /*_objectClasses.Add(new .(
                	classname: "object_vehicle_spawn_node",
                	color: .(0.719f, 0.494f, 0.982f),
                	visible: true,
                	icon: Icons.ICON_FA_CAR_SIDE));*/

                objectClasses.Add(new .(
                	classname: "ladder",
                	color: .(1.0f, 1.0f, 1.0f),
                	visible: true,
                	icon: Icons.ICON_FA_LEVEL_UP_ALT));

                objectClasses.Add(new .(
                	classname: "constraint",
                	color: .(0.975f, 0.407f, 1.0f),
                	visible: true,
                	icon: Icons.ICON_FA_LOCK));

                objectClasses.Add(new .(
                	classname: "object_effect",
                	color: .(1.0f, 0.45f, 0.0f),
                	visible: true,
                	icon: Icons.ICON_FA_FIRE));

                objectClasses.Add(new .(
                	classname: "trigger_region",
                	color: .(0.63f, 0.0f, 1.0f),
                	visible: true,
                	icon: Icons.ICON_FA_BORDER_NONE));

                /*_objectClasses.Add(new .(
                	classname: "object_bftp_node",
                	color: .(1.0f, 1.0f, 1.0f),
                	visible: true,
                	icon: Icons.ICON_FA_BOMB));*/

                objectClasses.Add(new .(
                	classname: "object_bounding_box",
                	color: .(1.0f, 1.0f, 1.0f),
                	visible: true,
                	icon: Icons.ICON_FA_CUBE));

                /*_objectClasses.Add(new .(
                	classname: "object_turret_spawn_node",
                	color: .(0.719f, 0.494f, 0.982f),
                	visible: true,
                	icon: Icons.ICON_FA_CROSSHAIRS));*/

                objectClasses.Add(new .(
                	classname: "obj_zone",
                	color: .(0.935f, 0.0f, 1.0f),
                	visible: true,
                	icon: Icons.ICON_FA_SEARCH_LOCATION));

                /*_objectClasses.Add(new .(
                    classname: "obj_patrol",
                    color: .(1.0f, 1.0f, 1.0f),
                    visible: true,
                    icon: Icons.ICON_FA_BINOCULARS));*/

                objectClasses.Add(new .(
                    classname: "object_dummy",
                    color: .(1.0f, 1.0f, 1.0f),
                    visible: true,
                    icon: Icons.ICON_FA_MEH_BLANK));

                /*_objectClasses.Add(new .(
                    classname: "object_raid_node",
                    color: .(0.719f, 0.494f, 0.982f),
                    visible: false,
                    icon: Icons.ICON_FA_CAR_CRASH));*/

                /*_objectClasses.Add(new .(
                    classname: "object_delivery_node",
                    color: .(0.719f, 0.494f, 0.982f),
                    visible: false,
                    icon: Icons.ICON_FA_SHIPPING_FAST));*/

                /*_objectClasses.Add(new .(
                    classname: "marauder_ambush_region",
                    color: .(1.0f, 1.0f, 1.0f),
                    visible: true,
                    icon: Icons.ICON_FA_USER_NINJA));*/

                /*_objectClasses.Add(new .(
                    classname: "object_activity_spawn",
                    color: .(1.0f, 1.0f, 1.0f),
                    visible: true,
                    icon: Icons.ICON_FA_SCROLL));*/

                /*_objectClasses.Add(new .(
                    classname: "object_mission_start_node",
                    color: .(0.719f, 0.494f, 0.982f),
                    visible: false,
                    icon: Icons.ICON_FA_MAP_MARKED));*/

                /*_objectClasses.Add(new .(
                    classname: "object_demolitions_master_node",
                    color: .(0.719f, 0.494f, 0.982f),
                    visible: false,
                    icon: Icons.ICON_FA_BOMB));*/

                /*_objectClasses.Add(new .(
                    classname: "object_restricted_area",
                    color: .(1.0f, 1.0f, 1.0f),
                    visible: true,
                    icon: Icons.ICON_FA_USER_SLASH));*/

                /*_objectClasses.Add(new .(
                    classname: "effect_streaming_node",
                    color: .(0.719f, 0.494f, 0.982f),
                    visible: false,
                    icon: Icons.ICON_FA_SPINNER));*/

                /*_objectClasses.Add(new .(
                    classname: "object_house_arrest_node",
                    color: .(0.719f, 0.494f, 0.982f),
                    visible: false,
                    icon: Icons.ICON_FA_USER_LOCK));*/

                /*_objectClasses.Add(new .(
                    classname: "object_area_defense_node",
                    color: .(0.719f, 0.494f, 0.982f),
                    visible: false,
                    icon: Icons.ICON_FA_USER_SHIELD));*/

                /*_objectClasses.Add(new .(
                    classname: "object_safehouse",
                    color: .(0.0f, 0.905f, 1.0f),
                    visible: true,
                    icon: Icons.ICON_FA_FIST_RAISED));*/

                /*_objectClasses.Add(new .(
                    classname: "object_convoy_end_point",
                    color: .(1.0f, 1.0f, 1.0f),
                    visible: true,
                    icon: Icons.ICON_FA_TRUCK_MOVING));*/

                /*_objectClasses.Add(new .(
                    classname: "object_courier_end_point",
                    color: .(1.0f, 1.0f, 1.0f),
                    visible: true,
                    icon: Icons.ICON_FA_FLAG_CHECKERED));*/

                /*_objectClasses.Add(new .(
                    classname: "object_riding_shotgun_node",
                    color: .(0.719f, 0.494f, 0.982f),
                    visible: false,
                    icon: Icons.ICON_FA_TRUCK_MONSTER));*/

                /*_objectClasses.Add(new .(
                    classname: "object_upgrade_node",
                    color: .(0.719f, 0.494f, 0.982f),
                    visible: false,
                    icon: Icons.ICON_FA_ARROW_UP));*/

                /*_objectClasses.Add(new .(
                    classname: "object_ambient_behavior_region",
                    color: .(1.0f, 1.0f, 1.0f),
                    visible: true,
                    icon: Icons.ICON_FA_TREE));*/

                /*_objectClasses.Add(new .(
                    classname: "object_roadblock_node",
                    color: .(0.719f, 0.494f, 0.982f),
                    visible: false,
                    icon: Icons.ICON_FA_HAND_PAPER));*/

                /*_objectClasses.Add(new .(
                    classname: "object_spawn_region",
                    color: .(1.0f, 1.0f, 1.0f),
                    visible: true,
                    icon: Icons.ICON_FA_USER_PLUS));*/

                objectClasses.Add(new .(
                    classname: "obj_light",
                    color: .(1.0f, 1.0f, 1.0f),
                    visible: true,
                    icon: Icons.ICON_FA_LIGHTBULB));

                CVar_ObjectClasses.Save();
            }
        }

        private ZoneObjectClass GetObjectClass(ZoneObject zoneObject)
        {
            for (ZoneObjectClass objectClass in CVar_ObjectClasses->Classes)
            {
                if (objectClass.Classname == zoneObject.Classname)
                {
                    return objectClass;
                }
            }

            return _unknownObjectClass;
        }

        private void CountObjectClassInstances()
        {
            for (ZoneObjectClass objectClass in CVar_ObjectClasses->Classes)
                objectClass.NumInstances = 0;

            for (Zone zone in Map.Zones)
            {
                for (ZoneObject zoneObject in zone.Objects)
                {
                    for (ZoneObjectClass objectClass in CVar_ObjectClasses->Classes)
                    {
                        if (objectClass.Classname == zoneObject.Classname)
                        {
                            objectClass.NumInstances++;
                            break;
                        }
                    }
                }
            }
        }
	}
}

//Holds various metadata on each object class like visibility, outliner icon, and bounding box color
[BonTarget]
public class ZoneObjectClass
{
    public String Classname = new .() ~delete _;
    public u32 NumInstances = 0;
    public Vec3 Color = .(1.0f, 1.0f, 1.0f);
    public bool Visible = true;
    public bool DrawSolid = false;
    public String Icon = new .() ~delete _;

    //Empty constructor for bon initialization
    public this()
    {

    }

    public this(StringView classname, Vec3 color, bool visible, char8* icon)
    {
        Classname.Set(classname);
        Color = color;
        Visible = visible;
        Icon.Set(StringView(icon));
        DrawSolid = false;
    }
}

[BonTarget]
public class ZoneObjectClasses : EditorObject
{
    public List<ZoneObjectClass> Classes = new .() ~DeleteContainerAndItems!(_);
}