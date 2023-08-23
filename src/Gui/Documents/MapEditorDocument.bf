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
        private ZoneObject _selectedObject;
        private Zone _selectedZone = null;

        //Deepest tree level the outliner will draw. Need to limit to prevent accidental stack overflow
        private const int MAX_OUTLINER_DEPTH = 7;

        private append Dictionary<Type, Type> _inspectorTypes;
        private append List<ZoneObjectClass> _objectClasses ~ClearAndDeleteItems(_);
        //Returned by GetObjectClass() when it can't find it
        private append ZoneObjectClass _unknownObjectClass = .(typeof(ZoneObject), "unknown", .(1.0f, 1.0f, 1.0f), true, Icons.ICON_FA_QUESTION);

        //TODO: Make these into cvars once they're added
        private bool _onlyShowPersistentObjects = false;
        //private bool _highlightHoveredZone = false; //TODO: Implement once multi zone maps are supported again
        //private float _zoneBoxHeight = 150.0f;
        private bool _highlightHoveredObject = false;
        private bool _autoMoveChildren = true; //Auto move children when the parent object is moved

        //Holds various metadata on each object class like visibility, outliner icon, and bounding box color
        private class ZoneObjectClass
        {
            public readonly Type ObjectType;
            public append String DisplayName;
            public u32 NumInstances = 0;
            public Vec3 Color = .(1.0f, 1.0f, 1.0f);
            public bool Visible = true;
            public bool DrawSolid = false;
            public char8* Icon = "";

            public this(Type objectType, StringView displayName, Vec3 color, bool visible, char8* icon)
            {
                ObjectType = objectType;
                DisplayName.Set(displayName);
                Color = color;
                Visible = visible;
                Icon = icon;
                DrawSolid = false;
            }
        }

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
            Territory findResult = ProjectDB.Find<Territory>(MapName);
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
            }

            //Don't redraw if this document isn't focused by the user. 
            _scene.Active = (this == gui.FocusedDocument);
            if (!_scene.Active)
                return;

            //Draw object bounding boxes
            Random rand = scope .(0);
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

            if (ImGui.BeginPopup("##ObjectFilterPopup"))
            {
                if (ImGui.Button("Show all types"))
                {
                    for (ZoneObjectClass objectClass in _objectClasses)
                    {
                        objectClass.Visible = true;
                    }
                }
                ImGui.SameLine();
                if (ImGui.Button("Hide all types"))
                {
                    for (ZoneObjectClass objectClass in _objectClasses)
                    {
                        objectClass.Visible = false;
                    }
                }
                
                ImGui.Checkbox("Only show persistent", &_onlyShowPersistentObjects);
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

                    for (ZoneObjectClass objectClass in _objectClasses)
                    {
                        ImGui.TableNextRow();

                        //Type
                        ImGui.TableNextColumn();
                        ImGui.Text(objectClass.DisplayName);

                        //Visible
                        ImGui.TableNextColumn();
                        ImGui.Checkbox(scope $"##Visible{objectClass.DisplayName}", &objectClass.Visible);

                        //Solid
                        ImGui.TableNextColumn();
                        ImGui.Checkbox(scope $"##Solid{objectClass.DisplayName}", &objectClass.DrawSolid);

                        //# of objects of this type
                        ImGui.TableNextColumn();
                        ImGui.Text(scope $"{objectClass.NumInstances} objects");

                        //Object color
                        ImGui.TableNextColumn();
                        ImGui.ColorEdit3(scope $"##Color{objectClass.DisplayName}", ref *(float[3]*)&objectClass.Color, .NoInputs | .NoLabel);
                    }

                    ImGui.EndTable();
                }
                ImGui.PopStyleColor(3);

                ImGui.EndPopup();
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

            bool selected = (obj == _selectedObject);
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
                if (obj == _selectedObject)
                    _selectedObject = null;
                else
                    _selectedObject = obj;
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
            if (_selectedObject == null)
                return;

            //Draw inspector for viewing and editing object properties. They're type specific classes which implement IZoneObjectInspector
            Type objectType = _selectedObject.GetType();
            if (_inspectorTypes.ContainsKey(objectType))
            {
                Type inspectorType = _inspectorTypes[objectType];
                if (inspectorType.GetMethod("Draw") case .Ok(MethodInfo methodInfo))
                {
                    methodInfo.Invoke(null, app, gui, _selectedObject).Dispose();
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
            //Note: I included SP only classes so they just need to be uncommented when they're eventually added
            _objectClasses.Add(new .(
				objectType: typeof(RfgMover),
				displayName: "rfg_mover",
				color: .(0.819f, 0.819f, 0.819f),
				visible: true,
				icon: Icons.ICON_FA_HOME));

            _objectClasses.Add(new .(
				objectType: typeof(CoverNode),
				displayName: "cover_node",
				color: .(1.0f, 0.0f, 0.0f),
				visible: false,
				icon: Icons.ICON_FA_SHIELD_ALT));

            _objectClasses.Add(new .(
				objectType: typeof(NavPoint),
				displayName: "navpoint",
				color: .(1.0f, 0.968f, 0.0f),
				visible: false,
				icon: Icons.ICON_FA_LOCATION_ARROW));

            _objectClasses.Add(new .(
				objectType: typeof(GeneralMover),
				displayName: "general_mover",
				color: .(1.0f, 0.664f, 0.0f),
				visible: true,
				icon: Icons.ICON_FA_CUBES));

            _objectClasses.Add(new .(
				objectType: typeof(PlayerStart),
				displayName: "player_start",
				color: .(0.591f, 1.0f, 0.0f),
				visible: true,
				icon: Icons.ICON_FA_STREET_VIEW));

            _objectClasses.Add(new .(
				objectType: typeof(MultiMarker),
				displayName: "multi_object_marker",
				color: .(0.000f, 0.759f, 1.0f),
				visible: true,
				icon: Icons.ICON_FA_MAP_MARKER));

            _objectClasses.Add(new .(
				objectType: typeof(Weapon),
				displayName: "weapon",
				color: .(1.0f, 0.0f, 0.0f),
				visible: true,
				icon: Icons.ICON_FA_CROSSHAIRS));

            _objectClasses.Add(new .(
				objectType: typeof(ActionNode),
				displayName: "object_action_node",
				color: .(0.719f, 0.494f, 0.982f),
				visible: true,
				icon: Icons.ICON_FA_RUNNING));

            /*_objectClasses.Add(new .(
				objectType: typeof(SquadSpawnNode),
				displayName: "object_squad_spawn_node",
				color: .(0.719f, 0.494f, 0.982f),
				visible: true,
				icon: Icons.ICON_FA_USERS));*/

            /*_objectClasses.Add(new .(
				objectType: typeof(NpcSpawnNode),
				displayName: "object_npc_spawn_node",
				color: .(0.719f, 0.494f, 0.982f),
				visible: true,
				icon: Icons.ICON_FA_USER));*/

            /*_objectClasses.Add(new .(
				objectType: typeof(GuardNode),
				displayName: "object_guard_node",
				color: .(0.719f, 0.494f, 0.982f),
				visible: true,
				icon: Icons.ICON_FA_SHIELD_ALT));*/

            /*_objectClasses.Add(new .(
				objectType: typeof(RoadPath),
				displayName: "object_path_road",
				color: .(0.719f, 0.494f, 0.982f),
				visible: true,
				icon: Icons.ICON_FA_ROAD));*/

            _objectClasses.Add(new .(
				objectType: typeof(ShapeCutter),
				displayName: "shape_cutter",
				color: .(1.0f, 1.0f, 1.0f),
				visible: true,
				icon: Icons.ICON_FA_CUT));

            _objectClasses.Add(new .(
				objectType: typeof(Item),
				displayName: "item",
				color: .(0.719f, 0.494f, 0.982f),
				visible: true,
				icon: Icons.ICON_FA_TOOLS));

            /*_objectClasses.Add(new .(
				objectType: typeof(VehicleSpawnNode),
				displayName: "object_vehicle_spawn_node",
				color: .(0.719f, 0.494f, 0.982f),
				visible: true,
				icon: Icons.ICON_FA_CAR_SIDE));*/

            _objectClasses.Add(new .(
				objectType: typeof(Ladder),
				displayName: "ladder",
				color: .(1.0f, 1.0f, 1.0f),
				visible: true,
				icon: Icons.ICON_FA_LEVEL_UP_ALT));

            _objectClasses.Add(new .(
				objectType: typeof(RfgConstraint),
				displayName: "constraint",
				color: .(0.975f, 0.407f, 1.0f),
				visible: true,
				icon: Icons.ICON_FA_LOCK));

            _objectClasses.Add(new .(
				objectType: typeof(ObjectEffect),
				displayName: "object_effect",
				color: .(1.0f, 0.45f, 0.0f),
				visible: true,
				icon: Icons.ICON_FA_FIRE));

            _objectClasses.Add(new .(
				objectType: typeof(TriggerRegion),
				displayName: "trigger_region",
				color: .(0.63f, 0.0f, 1.0f),
				visible: true,
				icon: Icons.ICON_FA_BORDER_NONE));

            /*_objectClasses.Add(new .(
				objectType: typeof(BftpNode),
				displayName: "object_bftp_node",
				color: .(1.0f, 1.0f, 1.0f),
				visible: true,
				icon: Icons.ICON_FA_BOMB));*/

            _objectClasses.Add(new .(
				objectType: typeof(ObjectBoundingBox),
				displayName: "object_bounding_box",
				color: .(1.0f, 1.0f, 1.0f),
				visible: true,
				icon: Icons.ICON_FA_CUBE));

            /*_objectClasses.Add(new .(
				objectType: typeof(TurretSpawnNode),
				displayName: "object_turret_spawn_node",
				color: .(0.719f, 0.494f, 0.982f),
				visible: true,
				icon: Icons.ICON_FA_CROSSHAIRS));*/

            _objectClasses.Add(new .(
				objectType: typeof(ObjZone),
				displayName: "obj_zone",
				color: .(0.935f, 0.0f, 1.0f),
				visible: true,
				icon: Icons.ICON_FA_SEARCH_LOCATION));

            /*_objectClasses.Add(new .(
	            objectType: typeof(ObjPatrol),
	            displayName: "obj_patrol",
	            color: .(1.0f, 1.0f, 1.0f),
	            visible: true,
	            icon: Icons.ICON_FA_BINOCULARS));*/

            _objectClasses.Add(new .(
	            objectType: typeof(ObjectDummy),
	            displayName: "object_dummy",
	            color: .(1.0f, 1.0f, 1.0f),
	            visible: true,
	            icon: Icons.ICON_FA_MEH_BLANK));

            /*_objectClasses.Add(new .(
	            objectType: typeof(RaidNode),
	            displayName: "object_raid_node",
	            color: .(0.719f, 0.494f, 0.982f),
	            visible: false,
	            icon: Icons.ICON_FA_CAR_CRASH));*/

            /*_objectClasses.Add(new .(
	            objectType: typeof(DeliveryNode),
	            displayName: "object_delivery_node",
	            color: .(0.719f, 0.494f, 0.982f),
	            visible: false,
	            icon: Icons.ICON_FA_SHIPPING_FAST));*/

            /*_objectClasses.Add(new .(
	            objectType: typeof(MarauderAmbushRegion),
	            displayName: "marauder_ambush_region",
	            color: .(1.0f, 1.0f, 1.0f),
	            visible: true,
	            icon: Icons.ICON_FA_USER_NINJA));*/

            /*_objectClasses.Add(new .(
	            objectType: typeof(ActivitySpawnNode),
	            displayName: "object_activity_spawn",
	            color: .(1.0f, 1.0f, 1.0f),
	            visible: true,
	            icon: Icons.ICON_FA_SCROLL));*/

            /*_objectClasses.Add(new .(
	            objectType: typeof(MissionStartNode),
	            displayName: "object_mission_start_node",
	            color: .(0.719f, 0.494f, 0.982f),
	            visible: false,
	            icon: Icons.ICON_FA_MAP_MARKED));*/

            /*_objectClasses.Add(new .(
	            objectType: typeof(DemolitionsMasterNode),
	            displayName: "object_demolitions_master_node",
	            color: .(0.719f, 0.494f, 0.982f),
	            visible: false,
	            icon: Icons.ICON_FA_BOMB));*/

            /*_objectClasses.Add(new .(
	            objectType: typeof(RestrictedArea),
	            displayName: "object_restricted_area",
	            color: .(1.0f, 1.0f, 1.0f),
	            visible: true,
	            icon: Icons.ICON_FA_USER_SLASH));*/

            /*_objectClasses.Add(new .(
	            objectType: typeof(EffectStreamingNode),
	            displayName: "effect_streaming_node",
	            color: .(0.719f, 0.494f, 0.982f),
	            visible: false,
	            icon: Icons.ICON_FA_SPINNER));*/

            /*_objectClasses.Add(new .(
	            objectType: typeof(HouseArrestNode),
	            displayName: "object_house_arrest_node",
	            color: .(0.719f, 0.494f, 0.982f),
	            visible: false,
	            icon: Icons.ICON_FA_USER_LOCK));*/

            /*_objectClasses.Add(new .(
	            objectType: typeof(AreaDefenseNode),
	            displayName: "object_area_defense_node",
	            color: .(0.719f, 0.494f, 0.982f),
	            visible: false,
	            icon: Icons.ICON_FA_USER_SHIELD));*/

            /*_objectClasses.Add(new .(
	            objectType: typeof(Safehouse),
	            displayName: "object_safehouse",
	            color: .(0.0f, 0.905f, 1.0f),
	            visible: true,
	            icon: Icons.ICON_FA_FIST_RAISED));*/

            /*_objectClasses.Add(new .(
	            objectType: typeof(ConvoyEndPoint),
	            displayName: "object_convoy_end_point",
	            color: .(1.0f, 1.0f, 1.0f),
	            visible: true,
	            icon: Icons.ICON_FA_TRUCK_MOVING));*/

            /*_objectClasses.Add(new .(
	            objectType: typeof(CourierEndPoint),
	            displayName: "object_courier_end_point",
	            color: .(1.0f, 1.0f, 1.0f),
	            visible: true,
	            icon: Icons.ICON_FA_FLAG_CHECKERED));*/

            /*_objectClasses.Add(new .(
	            objectType: typeof(RidingShotgunNode),
	            displayName: "object_riding_shotgun_node",
	            color: .(0.719f, 0.494f, 0.982f),
	            visible: false,
	            icon: Icons.ICON_FA_TRUCK_MONSTER));*/

            /*_objectClasses.Add(new .(
	            objectType: typeof(UpgradeNode),
	            displayName: "object_upgrade_node",
	            color: .(0.719f, 0.494f, 0.982f),
	            visible: false,
	            icon: Icons.ICON_FA_ARROW_UP));*/

            /*_objectClasses.Add(new .(
	            objectType: typeof(AmbientBehaviorRegion),
	            displayName: "object_ambient_behavior_region",
	            color: .(1.0f, 1.0f, 1.0f),
	            visible: true,
	            icon: Icons.ICON_FA_TREE));*/

            /*_objectClasses.Add(new .(
	            objectType: typeof(RoadblockNode),
	            displayName: "object_roadblock_node",
	            color: .(0.719f, 0.494f, 0.982f),
	            visible: false,
	            icon: Icons.ICON_FA_HAND_PAPER));*/

            /*_objectClasses.Add(new .(
	            objectType: typeof(SpawnRegion),
	            displayName: "object_spawn_region",
	            color: .(1.0f, 1.0f, 1.0f),
	            visible: true,
	            icon: Icons.ICON_FA_USER_PLUS));*/

            _objectClasses.Add(new .(
	            objectType: typeof(ObjLight),
	            displayName: "obj_light",
	            color: .(1.0f, 1.0f, 1.0f),
	            visible: true,
	            icon: Icons.ICON_FA_LIGHTBULB));
        }

        private ZoneObjectClass GetObjectClass(ZoneObject zoneObject)
        {
            Type type = zoneObject.GetType();
            for (ZoneObjectClass objectClass in _objectClasses)
            {
                if (objectClass.ObjectType == type)
                {
                    return objectClass;
                }
            }

            return _unknownObjectClass;
        }

        private void CountObjectClassInstances()
        {
            for (ZoneObjectClass objectClass in _objectClasses)
                objectClass.NumInstances = 0;

            for (Zone zone in Map.Zones)
            {
                for (ZoneObject zoneObject in zone.Objects)
                {
                    for (ZoneObjectClass objectClass in _objectClasses)
                    {
                        if (objectClass.ObjectType == zoneObject.GetType())
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