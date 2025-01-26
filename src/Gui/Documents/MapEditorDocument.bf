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
using Nanoforge.Rfg.Export;
using System.Linq;
using Nanoforge.Gui.Dialogs;
using Common.Math;

namespace Nanoforge.Gui.Documents
{
    [ReflectAll]
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
        public Scene Scene = null;
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
        private const int MAX_OUTLINER_DEPTH = 16;

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
        private ZoneObject _hoveredObject = null;
        public bool AutoMoveChildren = true; //Auto move children when the parent object is moved

        [BonTarget, ReflectAll]
        public class MapEditorSettings : EditorObject
        {
            public String MapExportPath = new .() ~delete _;
            public bool HighlightSelectedObject = true;
			public bool RotateBoundingBoxes = false;
			public bool DrawOrientationLines = true;
        }

		[BonTarget, ReflectAll]
		public class CameraSettings : EditorObject
		{
			public f32 Speed = 5.0f;
		}

		public static append CVar<MapEditorSettings> CVar_MapEditorSettings = .("Map Editor Settings");
        public static append CVar<CameraSettings> CVar_CameraSettings = .("Camera Settings");

        private bool _showMapExportFolderSelectorError = false;
        private append String _mapExportFolderSelectorError;

        [RegisterDialog]
        public append DeleteObjectConfirmationDialog DeleteConfirmationDialog;

        [RegisterDialog]
        public append CreateObjectDialog CreateObjectDialog;

        public append MessageBox OrphanObjectPopup = .("Orphan object?");
        public append MessageBox OrphanChildrenPopup = .("Orphan children?");

        //Used by the outliner to queue up actions that get run after the outliner is drawn. Don't want to do things like edit the child list of an object while iterating it.
        private struct ChangeParentAction
        {
            public ZoneObject Target;
            public ZoneObject NewParent;

            public this(ZoneObject target, ZoneObject newParent)
            {
                Target = target;
                NewParent = newParent;
            }
        }

        private append List<ChangeParentAction> _queuedActions;

        //Set this and the objects node will get expanded next frame
        private ZoneObject _expandObjectInOutliner = null;
        private ZoneObject _scrollToObjectInOutliner = null;

        //Set by the outliner context menu. Action is performed in Outliner() at the end of the from so the objects list isn't changed while iterating it.
        private ZoneObject _outlinerCloneObject = null;
        private ZoneObject _outlinerDeepCloneObject = null;

        private Vec4 _outlinerNodeHeaderColor = .(0.157f, 0.350f, 0.588f, 1.0f);
        private Vec4 _outlinerNodeHighlightColor = _outlinerNodeHeaderColor * 1.1f;

        private append String _outlinerSearch;
        private bool _outlinerSearchIgnoreCase = true;

        //Ray emitted by the mouse from the viewport this frame (regardless of whether the mouse clicked or not)
        public Ray? MouseRay = null;
        public bool DrawMousePickingRay = false;

        private append TranslationGizmo _translationGizmo;

        private bool _inspectorsInitialized = false;

        public this(StringView mapName)
        {
            SetupObjectClasses();
            MapName.Set(mapName);
            HasMenuBar = true;
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
            if (Scene != null)
            {
                _renderer.DestroyScene(Scene);
                Scene = null;
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
            defer { Scene.Active = true; }

            //Check if the map was already imported
            Territory findResult = NanoDB.Find<Territory>(MapName);
            if (findResult == null)
            {
                //Map needs to be imported
                Logger.Info("First time opening {} in this project. Importing...", MapName);
                Stopwatch importTimer = scope .(true);
                TotalImportTimer.Start();
                MapImporter mapImporter = new .();
                defer delete mapImporter;
                switch (mapImporter.ImportMap(MapName))
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
            if (Map.Load(renderer, Scene) case .Err(StringView err))
            {
                Logger.Error("An error occurred while loading {}. Halting loading. Error: {}", MapName, err);
                _loadFailure = true;
                _loadFailureReason = err;
                return;
            }

            //Temporary hardcoded settings for high lod terrain rendering. Will be removed once config gui is added
            Scene.PerFrameConstants.ShadeMode = 1;
            Scene.PerFrameConstants.DiffuseIntensity = 1.0f;
            Scene.PerFrameConstants.DiffuseColor = Colors.RGBA.White;

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
            Scene.Camera.TargetPosition = closestZonePos;
            Scene.Camera.TargetPosition.x += 125.0f;
            Scene.Camera.TargetPosition.y = 250.0f;
            Scene.Camera.TargetPosition.z -= 250.0f;
			Scene.Camera.Speed = CVar_CameraSettings.Value.Speed;

            CountObjectClassInstances();

            //Initialize inspectors. Some of them need to load fields from xtbls. Caused a noticeable hitch when the first object was selected when it was done in the main thread.
            for (var kv in _inspectorTypes)
            {
                Type inspectorType = kv.value;
                if (inspectorType.GetMethod("Init") case .Ok(MethodInfo methodInfo))
                {
                    //No value is returned. Its left up to the inspectors to disable fields or display a warning if a field couldn't be initialized.
                    methodInfo.Invoke(null).Dispose();
                }
                else
                {
                    Logger.Error(scope $"Failed to find '{inspectorType.GetName(.. scope .())}.Init()'");
                }
            }
            _inspectorsInitialized = true;

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
                Scene = _renderer.CreateScene(false);
                ThreadPool.QueueUserWorkItem(new () => { this.Load(app); });
            }
            if (Loading)
            {
                Fonts.FontXL.Push();
                {
                    String text = "Loading map";

                    ImGui.Vec2 windowPos = ImGui.GetWindowPos();
                    ImGui.Vec2 windowSize = ImGui.GetWindowSize();
                    ImGui.Vec2 textSize = ImGui.CalcTextSize(text);
                    ImGui.Vec2 newCursorPos = .(windowPos.x + (windowSize.x / 2.0f) - textSize.x, windowPos.y + (windowSize.y / 2.0f) - textSize.y);
                    ImGui.SetCursorPos(newCursorPos);

                    ImGui.Text(text);

                    newCursorPos.x += textSize.x;
                    ImGui.SetCursorPos(newCursorPos);
                    ImGui.SetCursorPosY(ImGui.GetCursorPosY() + 18.0f);
                    ImGui.SetCursorPosX(ImGui.GetCursorPosX() + 20.0f);
                    ImGui.Spinner("##LoadingMapSpinner", 25.0f, 7, .SecondaryTextColor);
                }
                Fonts.FontXL.Pop();
                return;
            }
            else if (_loadFailure)
            {
                ImGui.Text(scope $"Error while loading map: {_loadFailureReason}");
                return;
            }

            if (Scene != null)
            {
                //Camera should only handle input when the viewport is focused or mouse hovered. Otherwise it'll react while you're typing into the inspector/outliner
                Scene.Camera.InputEnabled = ImGui.IsWindowFocused() && ImGui.IsWindowHovered();

                //Draw line showing direction of sunlight
                /*_scene.PerFrameConstants.SunDirection = .(0.8f, -0.5f, -1.0f); //.(0.8f, -1.0f, -0.5f);
                f32 sunLineLength = 3000.0f;
                Vec3 sunLineStart = .(255.0f, 0.0f, -255.0f);
                Vec3 sunLineEnd = (-1.0f * _scene.PerFrameConstants.SunDirection.Normalized()) * sunLineLength + sunLineStart;
                _scene.DrawLine(sunLineStart, sunLineEnd, .(1.0f, 0.7f, 0.0f, 1.0f));*/

                //Update scene viewport size
                ImGui.Vec2 contentAreaSize;
                contentAreaSize.x = ImGui.GetWindowContentRegionMax().x - ImGui.GetWindowContentRegionMin().x;
                contentAreaSize.y = ImGui.GetWindowContentRegionMax().y - ImGui.GetWindowContentRegionMin().y;
                if (contentAreaSize.x > 0.0f && contentAreaSize.y > 0.0f)
                {
                    Scene.Resize((u32)contentAreaSize.x, (u32)contentAreaSize.y);
                }

                //Store initial position so we can draw buttons over the scene texture after drawing it
                ImGui.Vec2 sceneViewportPos = ImGui.GetCursorPos();

                //Render scene texture
                ImGui.PushStyleColor(.WindowBg, .(Scene.ClearColor.x, Scene.ClearColor.y, Scene.ClearColor.z, Scene.ClearColor.w));
                ImGui.Image(Scene.View, .(Scene.ViewWidth, Scene.ViewHeight));
                ImGui.PopStyleColor();

                //Set cursor pos to top left corner to draw buttons over scene texture
                ImGui.Vec2 adjustedPos = sceneViewportPos;
                adjustedPos.x += 10.0f;
                adjustedPos.y += 10.0f;
                ImGui.SetCursorPos(adjustedPos);

                DrawMenuBar(app, gui);

                Input input = app.GetResource<Input>();
                ImGui.Vec2 windowPos = ImGui.GetWindowPos();
                Vec2 adjustedViewportPos = .(sceneViewportPos.x, sceneViewportPos.y);
                adjustedViewportPos.x += windowPos.x;
                adjustedViewportPos.y += windowPos.y;

                UpdateMouseRay(input, adjustedViewportPos, .(Scene.ViewWidth, Scene.ViewHeight));
                UpdatePicking(input);
            }

            Keybinds(app);

            //Don't redraw if this document isn't focused by the user. 
            Scene.Active = (this == gui.FocusedDocument);
            if (!Scene.Active)
                return;

            UpdateDebugDraw(app);
            UpdateGizmos(app);
        }

        //When the mouse hovers the viewport this calculates a ray emitting from the mouse into the scene
        //This function doesn't do any picking collision detection.
        private void UpdateMouseRay(Input input, Vec2 viewportPos, Vec2 viewportSize)
        {
            Rect rect = .(.(viewportPos.x, viewportPos.y), .(viewportPos.x + Scene.ViewWidth, viewportPos.y + Scene.ViewHeight));
            bool mouseHoveringViewport = rect.IsPositionInRect(.(input.MousePosX, input.MousePosY));
            if (mouseHoveringViewport)
            {
                //Adjust the mouse pos to be relative the viewport
                Vec2 mousePos = .(input.MousePosX, input.MousePosY) - viewportPos;

                Camera3D camera = Scene.Camera;
                Vec3 mouseViewPos = .();
                mouseViewPos.x = (((2.0f * mousePos.x) / Scene.ViewWidth) - 1.0f) / camera.Projection.Vectors[0].x;
                mouseViewPos.y = -(((2.0f * mousePos.y) / Scene.ViewHeight) - 1.0f) / camera.Projection.Vectors[1].y;
                mouseViewPos.z = 1.0f;

                Mat4 pickRayToWorldMatrix = Mat4.Inverse(camera.View);
                Vec3 pickRayInWorldSpacePos = DirectXMath.Vec3TransformCoord(.Zero, pickRayToWorldMatrix);
                Vec3 pickRayWorldSpaceDir = DirectXMath.Vec3TransformNormal(mouseViewPos, pickRayToWorldMatrix).Normalized();

                f32 pickRayLength = 1000.0f;
                Vec3 pickRayStart = pickRayInWorldSpacePos;//camera.Position;
                Vec3 pickRayEnd = pickRayStart + (pickRayLength * pickRayWorldSpaceDir);

                MouseRay = Ray(pickRayStart, pickRayEnd);
            }
            else
            {
                MouseRay = null;
            }    
        }

        private void UpdatePicking(Input input)
        {
            if (input.MouseButtonPressed(.Left) && MouseRay.HasValue)
            {
                Stopwatch timer = scope .(startNow: true);

                //Figure out which objects bounding boxes the pick ray has collided with
                f32 closestObjectDistanceFromCamera = f32.PositiveInfinity;
                ZoneObject closestHitObject = null;
                for (ZoneObject obj in Map.Zones[0].Objects) //TODO: For SP we'll need to come up with some other solution to avoid needing to loop through every zone. Maybe do GPU picking
                {
                    Vec3 intersection = .Zero;
                    if (obj.BBox.IntersectsLine(MouseRay.Value, ref intersection))
                    {
                        f32 distanceFromCamera = (obj.Position - MouseRay.Value.Start).Length;
                        if (distanceFromCamera < closestObjectDistanceFromCamera)
                        {
                            closestObjectDistanceFromCamera = distanceFromCamera;
                            closestHitObject = obj;
                        }
                    }
                }

                timer.Stop();
                if (input.MouseButtonPressed(.Left) && !_translationGizmo.Hovering && closestHitObject != null)
                {
                    SelectedObject = closestHitObject;

#if DEBUG
                    String objectLabel = scope .();
                    if (!closestHitObject.Name.IsEmpty)
                        objectLabel.Set(closestHitObject.Name);
                    else
                        objectLabel.Set(closestHitObject.Classname);

                    Debug.WriteLine("Mouse picking done. Picked {} - {}, {}. Took {}ms | {}us", objectLabel, closestHitObject.Handle, closestHitObject.Num, (f32)timer.ElapsedMicroseconds / 1000.0f, timer.ElapsedMicroseconds);
#endif
                }
            }
        }

        private void UpdateGizmos(App app)
        {
            if (SelectedObject != null)
            {
                _translationGizmo.Update(app, this, SelectedObject);
            }
        }

        private void UpdateDebugDraw(App app)
        {
            //Draw object bounding boxes
            for (Zone zone in Map.Zones)
            {
                for (ZoneObject obj in zone.Objects)
                {
                    ZoneObjectClass objectClass = GetObjectClass(obj);
                    if (!objectClass.Visible)
                        continue;
                    if (_onlyShowPersistentObjects && !obj.Flags.HasFlag(.Persistent))
                        continue;

                    BoundingBox bbox = obj.BBox;

                    //Holding down T will display the secondary bbox instead of the primary one for objects that have one
                    if (ImGui.IsKeyDown(.T))
                    {
                        if (obj.GetType() == typeof(ObjectBoundingBox))
                        {
                            bbox = (obj as ObjectBoundingBox).LocalBBox;
                            bbox.Min += obj.Position;
                            bbox.Max += obj.Position;
                        }
                        else if (obj.GetType() == typeof(TriggerRegion))
                        {
                            bbox = (obj as TriggerRegion).LocalBBox;
                            bbox.Min += obj.Position;
                            bbox.Max += obj.Position;
                        }
                        else if (obj.GetType() == typeof(MultiMarker))
                        {
                            bbox = (obj as MultiMarker).LocalBBox;
                            bbox.Min += obj.Position;
                            bbox.Max += obj.Position;
                        }
                    }

                    if (SelectedObject == obj && CVar_MapEditorSettings.Value.HighlightSelectedObject)
                    {
                        //For the selected object change the color of the BBox over time and draw a line into the sky so its easier to find
                        Vec3 color = objectClass.Color;

                        f32 colorMagnitude = objectClass.Color.Length;
                        //Negative values used for brighter colors so they get darkened instead of lightened//Otherwise doesn't work on objects with white debug color
                        f32 multiplier = colorMagnitude > 0.85f ? -1.0f : 1.0f;
                        color.x = objectClass.Color.x + Math.Pow(Math.Sin(Scene.TotalTime * 2.0f), 2.0f) * multiplier;
                        color.y = objectClass.Color.y + Math.Pow(Math.Sin(Scene.TotalTime), 2.0f) * multiplier;
                        color.z = objectClass.Color.z + Math.Pow(Math.Sin(Scene.TotalTime), 2.0f) * multiplier;

                        //Keep color in a certain range so it stays visible against the terrain
                        f32 magnitudeMin = 0.20f;
                        f32 colorMin = 0.20f;
                        if (color.Length < magnitudeMin)
                        {
                            color.x = Math.Max(color.x, colorMin);
                            color.y = Math.Max(color.y, colorMin);
                            color.z = Math.Max(color.z, colorMin);
                        }

                        //Draw bbox
						if (CVar_MapEditorSettings.Value.RotateBoundingBoxes && obj.Orient.Enabled)
						{
							Scene.DrawBoxRotated(bbox.Min, bbox.Max, obj.Orient.Value, obj.Position, Vec4(color.x, color.y, color.z, 1.0f));
						}
						else
						{
							Scene.DrawBox(bbox.Min, bbox.Max, Vec4(color.x, color.y, color.z, 1.0f));
						}

                        //Calculate bottom center of box so we can draw a line from the bottom of the box into the sky
                        /*Vec3 lineStart;
                        lineStart.x = (bbox.Min.x + bbox.Max.x) / 2.0f;
                        lineStart.y = bbox.Min.y;
                        lineStart.z = (bbox.Min.z + bbox.Max.z) / 2.0f;
                        Vec3 lineEnd = lineStart;
                        lineEnd.y += 300.0f;
                        Scene.DrawLine(lineStart, lineEnd, Vec4(color.x, color.y, color.z, 1.0f));*/
                    }
                    else
                    {
                        Vec4 color = .(objectClass.Color.x, objectClass.Color.y, objectClass.Color.z, 1.0f);
						if (CVar_MapEditorSettings.Value.RotateBoundingBoxes && obj.Orient.Enabled)
						{
							Scene.DrawBoxRotated(bbox.Min, bbox.Max, obj.Orient.Value, obj.Position, color);
						}
						else
						{
							Scene.DrawBox(bbox.Min, bbox.Max, color);
						}
                    }

					//Draw lines indicating object orientation (red = right, green = up, blue = forward) on selected object
					if (SelectedObject == obj && CVar_MapEditorSettings.Value.DrawOrientationLines && obj.Orient.Enabled)
					{
						Vec3 right = obj.Orient.Value.Vectors[0];
						Vec3 up = obj.Orient.Value.Vectors[1];
						Vec3 forward = obj.Orient.Value.Vectors[2];
						Vec3 size = obj.BBox.Max - obj.BBox.Min;
						f32 orientLineScale = 2.0f; //How many times larger than the object orient lines should be
						f32 lineLength = orientLineScale * Math.Max(Math.Max(size.x, size.y), size.z); //Make lines equal length, as long as the widest side of the bbox

						Mat3 orient = obj.Orient.Value;
						Vec3 center = obj.Position;
						Scene.DrawLine(center, center + (obj.Orient.Value.Vectors[0] * lineLength), Vec4(1.0f, 0.0f, 0.0f, 1.0f), 10.0f); //Right
						Scene.DrawLine(center, center + (obj.Orient.Value.Vectors[1] * lineLength), Vec4(0.0f, 1.0f, 0.0f, 1.0f), 10.0f); //Up
						Scene.DrawLine(center, center + (obj.Orient.Value.Vectors[2] * lineLength), Vec4(0.0f, 0.0f, 1.0f, 1.0f), 10.0f); //Forward
					}
                }
            }

            //Highlight object hovered in outliner
            if (_highlightHoveredObject && _hoveredObject != null)
			{
                ZoneObjectClass objectClass = GetObjectClass(_hoveredObject);
                Vec3 color = .(0.7f, 0.0f, 0.5f);
                if (objectClass != null)
                {
                    f32 colorMagnitude = objectClass.Color.Length;
                    //Negative values used for brighter colors so they get darkened instead of lightened//Otherwise doesn't work on objects with white debug color
                    f32 multiplier = colorMagnitude > 0.85f ? -1.0f : 1.0f;
                    color.x = objectClass.Color.x + Math.Pow(Math.Sin(Scene.TotalTime * 2.0f), 2.0f) * multiplier;
                    color.y = objectClass.Color.y + Math.Pow(Math.Sin(Scene.TotalTime), 2.0f) * multiplier;
                    color.z = objectClass.Color.z + Math.Pow(Math.Sin(Scene.TotalTime), 2.0f) * multiplier;
                }

                //Keep color in a certain range so it stays visible against the terrain
                f32 magnitudeMin = 0.20f;
                f32 colorMin = 0.20f;
                if (color.Length < magnitudeMin)
                {
                    color.x = Math.Max(color.x, colorMin);
                    color.y = Math.Max(color.y, colorMin);
                    color.z = Math.Max(color.z, colorMin);
                }

                Scene.DrawBoxSolid(_hoveredObject.BBox.Min, _hoveredObject.BBox.Max, .(color.x, color.y, color.z, 1.0f));
            }

            if (MouseRay.HasValue && DrawMousePickingRay)
            {
                Vec4 pickRayColor = .(1.0f, 0.0f, 1.0f, 1.0f);
                Scene.DrawLine(MouseRay.Value.Start, MouseRay.Value.End, pickRayColor);
            }
        }

        private void DrawMenuBar(App app, Gui gui)
        {
            ImGui.SetNextItemWidth(Scene.ViewWidth);
            ImGui.PushStyleVar(.WindowPadding, .(8.0f, 8.0f)); //Must manually set padding here since the parent window has padding disabled to get the viewport flush with the window border.
            defer { ImGui.PopStyleVar(); }
            bool openScenePopup = false;
            bool openCameraPopup = false;
            bool openExportPopup = false;

            //TODO: Implement the menu bar buttons and the camera + scene popups. Put the camera and scene popups in separate functions
            if (ImGui.BeginMenuBar())
            {
                if (ImGui.BeginMenu("Editor"))
                {
                    if (ImGui.MenuItem("Scene"))
                        openScenePopup = true;
                    if (ImGui.MenuItem("Camera"))
                        openCameraPopup = true;

                    ImGui.Separator();

                    if (ImGui.BeginMenu("Settings"))
                    {
                        if (ImGui.MenuItem("Auto move children", null, &AutoMoveChildren))
                        {

                        }
                        ImGui.TooltipOnPrevious("Automatically move child objects by the same amount when moving their parents");

                        if (ImGui.MenuItem("Highlight hovered objects in outliner", null, &_highlightHoveredObject))
                        {

                        }

                        if (ImGui.MenuItem("Only show persistent objects", null, &_onlyShowPersistentObjects))
                        {

                        }
                        ImGui.TooltipOnPrevious("Only display objects with the 'Persistent' flag set");

                        if (ImGui.MenuItem("Vary color of selected object", null, &CVar_MapEditorSettings.Value.HighlightSelectedObject))
                        {

                        }
                        ImGui.TooltipOnPrevious("Vary the color of the selected object to make it more clear");

						if (ImGui.MenuItem("Rotate bounding boxes", null, &CVar_MapEditorSettings.Value.RotateBoundingBoxes))
						{

						}
						ImGui.TooltipOnPrevious("Rotate bounding boxes when drawing them. They get rotated to whatever the 'Orient' value gets set to. Requires the object to have the orient field enabled. Useful for objects that don't have a mesh.");

						if (ImGui.MenuItem("Draw orientation lines", null, &CVar_MapEditorSettings.Value.DrawOrientationLines))
						{

						}
						ImGui.TooltipOnPrevious("Draw lines indicating which direction an object is rotated in");

                        ImGui.EndMenu();
                    }

                    ImGui.EndMenu();
                }

                if (ImGui.BeginMenu("Object"))
                {
                    bool canClone = (_selectedObject != null);
                    bool canDelete = (_selectedObject != null);
                    bool canRemoveWorldAnchors = (_selectedObject != null) && _selectedObject.GetType() == typeof(RfgMover);
                    bool canRemoveDynamicLinks = (_selectedObject != null) && _selectedObject.GetType() == typeof(RfgMover);
                    bool hasChildren = (_selectedObject != null) && _selectedObject.Children.Count > 0;
                    bool hasParent = (_selectedObject != null) && _selectedObject.Parent != null;

                    if (ImGui.MenuItem("Clone", "Ctrl + D", null, canClone))
                    {
                        ZoneObject clone = ShallowCloneObject(SelectedObject);
                        _scrollToObjectInOutliner = clone;
                        _selectedObject = clone;
                    }
                    if (ImGui.MenuItem("Deep clone", "Ctrl + Shift + D", null, canClone))
                    {
                        ZoneObject clone = DeepCloneObject(SelectedObject);
                        _scrollToObjectInOutliner = clone;
                        _selectedObject = clone;
                    }

                    ImGui.Separator();
                    mixin CreateObjectMenuItem(char8* displayName, bool disabled = false, ZoneObject parent = null)
                    {
                        ImGui.BeginDisabled(disabled);
                        if (ImGui.MenuItem(displayName))
                        {
                            OpenObjectCreationDialog(StringView(displayName), parent);
                        }
                        ImGui.EndDisabled();
                        if (disabled)
                        {
                            ImGui.TooltipOnPrevious(scope $"Nanoforge can't create these from scratch yet. Copy an existing {StringView(displayName)} instead and modify it."..EnsureNullTerminator());
                        }
                    }
                    if (ImGui.BeginMenu("Create object..."))
                    {
                        CreateObjectMenuItem!("obj_zone");
                        CreateObjectMenuItem!("object_bounding_box");
                        CreateObjectMenuItem!("object_dummy");
                        CreateObjectMenuItem!("player_start");
                        CreateObjectMenuItem!("trigger_region");
                        CreateObjectMenuItem!("object_mover", disabled: true);
                        CreateObjectMenuItem!("general_mover", disabled: true);
                        CreateObjectMenuItem!("rfg_mover", disabled: true);
                        CreateObjectMenuItem!("shape_cutter");
                        CreateObjectMenuItem!("object_effect");
                        CreateObjectMenuItem!("item");
                        CreateObjectMenuItem!("weapon");
                        CreateObjectMenuItem!("ladder");
                        CreateObjectMenuItem!("obj_light");
                        CreateObjectMenuItem!("multi_object_marker");
                        CreateObjectMenuItem!("multi_object_flag");

                        ImGui.EndMenu();
                    }
                    if (ImGui.BeginMenu("Create child object...", SelectedObject != null))
                    {
                        CreateObjectMenuItem!("obj_zone");
                        CreateObjectMenuItem!("object_bounding_box");
                        CreateObjectMenuItem!("object_dummy");
                        CreateObjectMenuItem!("player_start");
                        CreateObjectMenuItem!("trigger_region");
                        CreateObjectMenuItem!("object_mover", disabled: true);
                        CreateObjectMenuItem!("general_mover", disabled: true);
                        CreateObjectMenuItem!("rfg_mover", disabled: true);
                        CreateObjectMenuItem!("shape_cutter");
                        CreateObjectMenuItem!("object_effect");
                        CreateObjectMenuItem!("item");
                        CreateObjectMenuItem!("weapon");
                        CreateObjectMenuItem!("ladder");
                        CreateObjectMenuItem!("obj_light");
                        CreateObjectMenuItem!("multi_object_marker");
                        CreateObjectMenuItem!("multi_object_flag");

                        ImGui.EndMenu();
                    }

                    ImGui.Separator();
                    if (ImGui.MenuItem("Delete object", "Delete", null, canDelete))
                    {
                        OpenDeleteObjectDialog(_selectedObject);
                    }

                    ImGui.Separator();
                    if (ImGui.MenuItem("Orphan object", null, null, hasParent))
                    {
                        OpenOrphanObjectConfirmationDialog(_selectedObject);
                    }
                    if (ImGui.MenuItem("Orphan children", null, null, hasChildren))
                    {
                        OpenOrphanChildrenConfirmationDialog(_selectedObject);
                    }

                    /*ImGui.Separator();
                    if (ImGui.MenuItem("Remove world anchors", "Ctrl + B", null, canRemoveWorldAnchors))
                    {
                        //RemoveWorldAnchors(selectedObject_);
                    }
                    if (ImGui.MenuItem("Remove dynamic links", "Ctrl + N", null, canRemoveDynamicLinks))
                    {
                        //RemoveDynamicLinks(selectedObject_);
                    }*/

                    ImGui.EndMenu();
                }

                if (ImGui.MenuItem("Export"))
                {
                    openExportPopup = true;
                }

                ImGui.EndMenuBar();
            }

            //Have to open the popup in the same scope as BeginPopup(), can't do it in the menu item result. Annoying restriction for imgui popups.
            if (openScenePopup)
                ImGui.OpenPopup("##ScenePopup");
            if (openCameraPopup)
                ImGui.OpenPopup("##CameraPopup");
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
            if (ImGui.BeginPopup("##CameraPopup"))
            {
				Fonts.FontL.Push();
				ImGui.Text(scope String(Icons.ICON_FA_CAMERA)..Append(" Camera")..EnsureNullTerminator());
				Fonts.FontL.Pop();

				//Sync cvar with camera speed in case it was changed with the scrollbar
				if (CVar_CameraSettings.Value.Speed != Scene.Camera.Speed)
				{
					CVar_CameraSettings.Value.Speed = Scene.Camera.Speed;
					CVar_CameraSettings.Save();
				}

                f32 fov = Scene.Camera.FovDegrees;
                /*f32 nearPlane = Scene.Camera.GetNearPlane();*/
                /*f32 farPlane = Scene.Camera.GetFarPlane();*/

                if (ImGui.Button("0.1")) CVar_CameraSettings.Value.Speed = 0.1f;
                ImGui.SameLine();
                if (ImGui.Button("1.0")) CVar_CameraSettings.Value.Speed = 1.0f;
                ImGui.SameLine();
                if (ImGui.Button("10.0")) CVar_CameraSettings.Value.Speed = 10.0f;
                ImGui.SameLine();
                if (ImGui.Button("25.0")) CVar_CameraSettings.Value.Speed = 25.0f;
                ImGui.SameLine();
                if (ImGui.Button("50.0")) CVar_CameraSettings.Value.Speed = 50.0f;
                ImGui.SameLine();
                if (ImGui.Button("100.0")) CVar_CameraSettings.Value.Speed = 100.0f;

                if (ImGui.InputFloat("Speed", &CVar_CameraSettings.Value.Speed))
                {
                    CVar_CameraSettings.Save();
                }
                ImGui.InputFloat("Sprint multiplier", &Scene.Camera.SprintMultiplier);

                if (ImGui.SliderFloat("Fov", &fov, 40.0f, 120.0f))
                    Scene.Camera.FovDegrees = fov;
                /*if (ImGui.InputFloat("Near plane", &nearPlane))
                    Scene.Camera.SetNearPlane(nearPlane);
                if (ImGui.InputFloat("Far plane", &farPlane))
                    Scene.Camera.SetFarPlane(farPlane);*/
                ImGui.InputFloat("Look sensitivity", &Scene.Camera.LookSensitivity);

                if (ImGui.InputVec3("Position", ref Scene.Camera.Position))
                {
                    Scene.Camera.UpdateViewMatrix();
                }

                //Sync camera speed with cvar
                if (Scene.Camera.Speed != CVar_CameraSettings.Value.Speed)
                {
                    Scene.Camera.Speed = CVar_CameraSettings.Value.Speed;
                	CVar_CameraSettings.Save();
                }

                /*gui::LabelAndValue("Pitch:", std::to_string(Scene.Camera.GetPitchDegrees()));
                gui::LabelAndValue("Yaw:", std::to_string(Scene.Camera.GetYawDegrees()));
                if (ImGui.Button("Zero"))
                {
                    Scene.Camera.yawRadians_ = ToRadians(180.0f);
                    Scene.Camera.UpdateProjectionMatrix();
                    Scene.Camera.UpdateViewMatrix();
                }*/
                ImGui.EndPopup();
            }

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
                        ThreadPool.QueueUserWorkItem(new () =>
						{
                            MapExporter exporter = new .();
                            defer delete exporter;
							exporter.ExportMap(Map, CVar_MapEditorSettings->MapExportPath);
						});
                        ImGui.CloseCurrentPopup();
                    }
                }

                ImGui.EndPopup();
            }
        }

        private void Keybinds(App app)
        {
            if (Loading || !Input.KeysEnabled)
            {
                return;
            }

            Input input = app.GetResource<Input>();
            if (input.ControlDown)
            {
                if (SelectedObject != null && input.ShiftDown && input.KeyPressed(.D) && !ImGui.IsAnyItemActive()) //Ctrl + Shift + D
                {
                    ZoneObject clone = DeepCloneObject(SelectedObject);
                    _scrollToObjectInOutliner = clone;
                    _selectedObject = clone;
                }
                else if (SelectedObject != null && input.KeyPressed(.D) && !ImGui.IsAnyItemActive()) //Ctrl + D
                {
                    ZoneObject clone = ShallowCloneObject(SelectedObject);
                    _scrollToObjectInOutliner = clone;
                    _selectedObject = clone;
                }
                else if (SelectedObject != null && input.ShiftDown && input.KeyPressed(.Spacebar) && !ImGui.IsAnyItemActive()) //Ctrl + Shift + Space
                {
                    OpenObjectCreationDialog("object_dummy", SelectedObject); //Create object as child of the selected object
                }
                else if (input.KeyPressed(.Spacebar) && !ImGui.IsAnyItemActive()) //Ctrl + Space
                {
                    OpenObjectCreationDialog(); //Create object with no parent
                }
            }
            else
            {
                if (input.KeyPressed(.Delete) && !ImGui.IsAnyItemActive())
                {
                    if (_selectedObject != null)
					{
                        OpenDeleteObjectDialog(_selectedObject);
					}
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

            if (ImGui.InputText("##Search", _outlinerSearch))
            {

            }

            ImGui.Vec2 searchButtonSize = .(30.0f, 26.0f);
            ImGui.SameLine();
            ImGui.PushStyleVar(.FramePadding, .(0.0f, 6.0f));
            Fonts.FontL.Push();
            if (ImGui.ToggleButton(Icons.ICON_VS_CASE_SENSITIVE, &_outlinerSearchIgnoreCase, searchButtonSize))
            {

            }
            Fonts.FontL.Pop();
            ImGui.PopStyleVar(1);
            ImGui.TooltipOnPrevious("Match case");

            ImGui.SameLine();
            ImGui.PushStyleVar(.FramePadding, .(0.0f, 6.0f));
            Fonts.FontL.Push();
            if (ImGui.Button(Icons.ICON_VS_X, searchButtonSize))
            {
                _outlinerSearch.Set("");
            }
            Fonts.FontL.Pop();
            ImGui.PopStyleVar(1);
            ImGui.TooltipOnPrevious("Clear search");

            //Clear so if no object is hovered this frame it doesn't keep highlighting the last object that was hovered
            _hoveredObject = null;

            //Set custom highlight colors for the table
            ImGui.PushStyleColor(.Header, _outlinerNodeHeaderColor);
            ImGui.PushStyleColor(.HeaderHovered, _outlinerNodeHighlightColor);
            ImGui.PushStyleColor(.HeaderActive, _outlinerNodeHighlightColor);
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
                            Outliner_DrawObjectNode(gui, obj, depth + 1);
                        }
                    }

                    if (treeNodeOpen)
	                    ImGui.TreePop();
                }

                ImGui.EndTable();
            }

            ImGui.PopStyleColor(3);

            //Run queued actions. They're run now so we don't modify the object tree while iterating it.
            for (ChangeParentAction action in _queuedActions)
            {
                //Don't allow parent to be made a descendant of its descendants. Will make circular loop. There's ways we could handle this in code but for now I'd rather just not allow it.
                if (IsObjectDescendant(action.Target, action.NewParent))
                {
                    //At the moment swapping child and parent this way isn't allowed
                    Logger.Warning("Attempted to swap parent and child in outliner. Blocked. Parent: [{}, {}]. Child: [{}, {}]", action.NewParent.Handle, action.NewParent.Num, action.Target.Handle, action.Target.Num);
                    continue;
                }
                if (action.Target.Parent != null)
                {
                    action.Target.Parent.Children.Remove(action.Target);
                    action.Target.Parent = null;
                }
                action.Target.Parent = action.NewParent;
                action.NewParent.Children.Add(action.Target);

                //Scroll to and expand the node of the new parent
                _expandObjectInOutliner = action.NewParent;
                _scrollToObjectInOutliner = action.NewParent;
                UnsavedChanges = true;
            }
            _queuedActions.Clear();

            if (_outlinerCloneObject != null)
            {
                ZoneObject clone = ShallowCloneObject(_outlinerCloneObject);
                _scrollToObjectInOutliner = clone;
                SelectedObject = clone;
                _outlinerCloneObject = null;
            }
            if (_outlinerDeepCloneObject != null)
            {
                ZoneObject clone = DeepCloneObject(_outlinerDeepCloneObject);
                _scrollToObjectInOutliner = clone;
                SelectedObject = clone;
                _outlinerDeepCloneObject = null;
            }
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

        private bool Outliner_ObjectOrChildrenMatchSearch(ZoneObject obj)
        {
            ZoneObjectClass objectClass = GetObjectClass(obj);
            if (objectClass.Visible && (_outlinerSearch.IsEmpty || _outlinerSearch.IsWhiteSpace))
            {
                return true;
            }

            if (objectClass.Visible && obj.Name.Contains(_outlinerSearch, _outlinerSearchIgnoreCase))
            {
                return true;
            }


            for (ZoneObject child in obj.Children)
            {
                if (Outliner_ObjectOrChildrenMatchSearch(child))
                {
                    return true;
                }
            }

            return false;
        }

        private void Outliner_DrawObjectNode(Gui gui, ZoneObject obj, int depth)
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

            bool objectOrChildrenMatchSearch = Outliner_ObjectOrChildrenMatchSearch(obj);
            if (!objectOrChildrenMatchSearch)
            {
                return;
            }

            ImGui.TableNextRow();
            ImGui.TableNextColumn();

            bool hasChildren = obj.Children.Count > 0;
            bool hasParent = obj.Parent != null;

            ImGui.PushID(Internal.UnsafeCastToPtr(obj));
            if (_expandObjectInOutliner == obj) //Open node if set
            {
                ImGui.SetNextItemOpen(true);
                _expandObjectInOutliner = null;
            }

            //Draw node
            f32 nodeXPos = ImGui.GetCursorPosX(); //Store position of the node for drawing the node icon later
            bool nodeOpen = ImGui.TreeNodeEx(label.CStr(), .SpanAvailWidth | .OpenOnDoubleClick | .OpenOnArrow | (hasChildren ? .None : .Leaf) | (selected ? .Selected : .None));

            if (_scrollToObjectInOutliner == obj) //Scroll to node if set
            {
                ImGui.ScrollToItem();
                _scrollToObjectInOutliner = null;
            }
            if (ImGui.IsItemClicked(.Left) || ImGui.IsItemClicked(.Right))
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
                _hoveredObject = obj;
            }
            Outliner_DrawContextMenu(gui, obj);

            //When drag and drop starts record object being dragged
            if (ImGui.BeginDragDropSource())
            {
                void* objPtr = Internal.UnsafeCastToPtr(obj); //Dear ImGui makes a copy of the data passed to it. We don't want a copy of the object so we make a copy of a pointer holding its address.
                ImGui.SetDragDropPayload("Outliner_ObjectDragDrop", &objPtr, sizeof(void*));
                ImGui.EndDragDropSource();
            }

            //Handle object getting dropped. Changes the target object the source objects new parent.
            if (ImGui.BeginDragDropTarget())
            {
                //Queue up a parent change action
                ImGui.Payload* peekPayload = ImGui.GetDragDropPayload();
                void** objPtr = (void**)peekPayload.Data;
                ZoneObject objectBeingDropped = (ZoneObject)Internal.UnsafeCastToObject(*objPtr);
                bool isValidDropTarget = !IsObjectDescendant(objectBeingDropped, obj); //Can't become your parents ancestor

                if (isValidDropTarget && ImGui.AcceptDragDropPayload("Outliner_ObjectDragDrop",  .AcceptNoDrawDefaultRect | .AcceptNoPreviewTooltip) != null)
                {
                    if (objectBeingDropped.Parent != obj)
                    {
                        _queuedActions.Add(.(target: objectBeingDropped, newParent: obj));
                    }
                }

                if (!isValidDropTarget) //Highlight invalid target nodes red
                {
                    ImGui.Vec4 invalidTargetHighlightColor = .(1.0f, 0.0f, 0.0f, 0.5f);
                    ImGui.GetForegroundDrawList().AddRectFilled(ImGui.GetItemRectMin(), ImGui.GetItemRectMax(), ImGui.GetColorU32(invalidTargetHighlightColor));
                }
                else if (!hasChildren) //For some reason non leaf nodes don't get highlighted with drag and drop. Little hack to fix it for the time being.
                {
                    ImGui.Vec4 highlightColor = _outlinerNodeHighlightColor;
                    highlightColor.w = 0.5f; //Transparent so it doesn't block the text. Doesn't look 100% like the normal highlight but close enough. Couldn't figure out a way to draw behind the text (background draw list goes under the window)
                    ImGui.GetForegroundDrawList().AddRectFilled(ImGui.GetItemRectMin(), ImGui.GetItemRectMax(), ImGui.GetColorU32(highlightColor));
                }

                ImGui.EndDragDropTarget();
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
                    Outliner_DrawObjectNode(gui, child, depth + 1);
                }
                ImGui.TreePop();
            }
            ImGui.PopID();
        }

        //Recursively iterates child tree of rootObject and returns true if target is found.
        private bool IsObjectDescendant(ZoneObject rootObject, ZoneObject target)
        {
            for (ZoneObject child in rootObject.Children)
            {
                if (child == target)
                {
                    return true;
                }
                if (IsObjectDescendant(child, target))
                {
                    return true;
                }
            }

            return false;
        }

        private void Outliner_DrawContextMenu(Gui gui, ZoneObject obj)
        {
            if (ImGui.BeginPopupContextItem())
            {
                if (obj == null)
                {
                    ImGui.Text("No object selected...");
                    ImGui.EndPopup();
                    return;
                }

                readonly bool hasParent = obj.Parent != null;
                readonly bool hasChildren = obj.Children.Count > 0;

                var objClass = GetObjectClass(obj);
                String objLabel = scope .();
                if (!obj.Name.IsEmpty)
                    objLabel.Set(obj.Name);
                else
                    objLabel.Set(obj.Classname);

                ImGui.TextColored(.(objClass.Color.x, objClass.Color.y, objClass.Color.z, 1.0f), objLabel);
                ImGui.Separator();

                //Get list of open maps for 'Clone to...' and 'Deep clone to...' options
                List<GuiDocumentBase> otherMaps = gui.Documents.Select((doc) => doc)
					                                           .Where((doc) => doc.GetType() == typeof(MapEditorDocument) && doc != this)
					                                           .ToList(.. new .());
                defer delete otherMaps;

                if (ImGui.Selectable("Clone"))
                {
                    _outlinerCloneObject = obj;
                }

                if (ImGui.BeginMenu("Clone to...", enabled: otherMaps.Count > 0))
                {
                    for (var doc in otherMaps)
                    {
                        //Note: Since gui rendering and logic is single threaded we can perform these clones immediately instead of waiting like we do with copies on the same map
                        MapEditorDocument otherMap = (MapEditorDocument)doc;
                        String otherMapLabel = scope $"{doc.Title}{otherMap.Loading ? " - Loading..." : ""}";
                        if (ImGui.MenuItem(otherMapLabel, null, null, !otherMap.Loading))
                        {
                            ZoneObject newObject = otherMap.ShallowCloneObject(obj);
                            otherMap.SelectedObject = newObject;
                            otherMap._scrollToObjectInOutliner = newObject;
                            newObject.Parent = null;
                            newObject.Children.Clear();
                        }
                    }
                    ImGui.EndMenu();
                }

                if (ImGui.Selectable("Deep clone"))
                {
                    _outlinerDeepCloneObject = obj;
                }

                if (ImGui.BeginMenu("Deep clone to...", enabled: otherMaps.Count > 0))
                {
                    for (var doc in otherMaps)
                    {
                        //Note: Since gui rendering and logic is single threaded we can perform these clones immediately instead of waiting like we do with copies on the same map
                        MapEditorDocument otherMap = (MapEditorDocument)doc;
                        String otherMapLabel = scope $"{doc.Title}{otherMap.Loading ? " - Loading..." : ""}";
                        if (ImGui.MenuItem(otherMapLabel, null, null, !otherMap.Loading))
                        {
                            ZoneObject newObject = otherMap.DeepCloneObject(obj);
                            otherMap.SelectedObject = newObject;
                            otherMap._scrollToObjectInOutliner = newObject;
                            newObject.Parent = null;
                            newObject.Children.Clear();
                        }
                    }
                    ImGui.EndMenu();
                }

                ImGui.Separator();
                if (ImGui.Selectable("Create object"))
                {
                    OpenObjectCreationDialog();
                }

                using (ImGui.DisabledBlock(obj == null))
                {
                    if (ImGui.Selectable("Create child object"))
                    {
                        OpenObjectCreationDialog("object_dummy", obj);
                    }
                }

                ImGui.Separator();
                ImGui.BeginDisabled(!hasParent);
                if (ImGui.Selectable("Orphan"))
                {
                    OpenOrphanObjectConfirmationDialog(obj);
                }
                ImGui.EndDisabled();

                ImGui.BeginDisabled(!hasChildren);
                if (ImGui.Selectable("Orphan children"))
                {
                    OpenOrphanChildrenConfirmationDialog(obj);
                }
                ImGui.EndDisabled();

                ImGui.Separator();
                if (ImGui.Selectable("Delete"))
                {
                    OpenDeleteObjectDialog(obj);
                }

                ImGui.EndPopup();
            }
        }

        public override void Inspector(App app, Gui gui)
        {
            if (SelectedObject == null)
                return;

            if (!_inspectorsInitialized)
            {
                ImGui.TextWrapped("Inspectors not initialized! Check the log.");
            }

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

        //Trigger the deletion confirmation dialog to appear and setup a delegate to run on close that'll perform the deletion if confirmed.
        private void OpenDeleteObjectDialog(ZoneObject object)
        {
            DeleteConfirmationDialog.Show(object, new (dialogResult) =>
			{
                if (dialogResult == .Yes)
                {
                    DeleteObject(object, DeleteConfirmationDialog.ParentAdoptsChildren);
                }
			});
        }

        private void OpenOrphanObjectConfirmationDialog(ZoneObject object)
        {
            OrphanObjectPopup.Show("Are you sure you'd like to make this object an orphan?", .YesCancel, new (dialogResult) =>
			{
                if (dialogResult == .Yes)
                {
                    object.Parent.Children.Remove(object);
                    object.Parent = null;
                    SelectedObject = object;
                    UnsavedChanges = true;
                }
			});
        }

        private void OpenOrphanChildrenConfirmationDialog(ZoneObject object)
        {
            OrphanChildrenPopup.Show("Are you sure you'd like to orphan all the children of this object? You'll need to manually add the children to parent if you want to undo this.", .YesCancel, new (dialogResult) =>
        	{
                if (dialogResult == .Yes)
                {
                    for (ZoneObject child in object.Children)
                    {
                        child.Parent = null;
                    }
                    object.Children.Clear();
                    UnsavedChanges = true;
                }
        	});
        }

        //parentAdoptsChildren causes the deleted objects parent to adopt its children. E.g. A -> B -> { C, D } We delete B and now A is the parent of C and D
        private void DeleteObject(ZoneObject object, bool parentAdoptsChildren)
        {
            if (_selectedObject == object)
            {
                _selectedObject = null;
            }

            //Update child objects parent reference
            for (ZoneObject child in object.Children)
            {
                if (parentAdoptsChildren && object.Parent != null)
                {
                    //Parent of deleted object adopts children
                    child.Parent = object.Parent;
                }
                else
				{
                    child.Parent = null;
				}
            }

            //Remove from parents children list
            if (object.Parent != null)
            {
                object.Parent.Children.Remove(object);
            }

            //Remove from objects list in zone
            for (Zone zone in Map.Zones)
            {
                if (zone.Objects.Contains(object))
                {
                    zone.Objects.Remove(object);
                    break;
                }
            }

            //Delete renderer data if the object has any
            if (object.RenderObject != null)
            {
                Scene.DeleteRenderObject(object.RenderObject);
            }

            //Delete from NanoDB
            NanoDB.DeleteObject(object);

            UnsavedChanges = true;
        }

        private u32 GetNewObjectHandle()
        {
            //Find largest handle in use and add one
            u32 largestHandle = 0;
            for (Zone zone in Map.Zones)
            {
                for (ZoneObject obj in zone.Objects)
                {
                    if (obj.Handle > largestHandle)
                    {
                        largestHandle = obj.Handle;
                    }
                }
            }

            //Error if handle is out of valid range
            if (largestHandle == u32.MaxValue)
            {
                Logger.Error("Failed to find new object handle. Returning 0xFFFFFFFF");
                return u32.MaxValue;
            }

            //TODO: See if we can start at 0 instead. All vanilla game handles are very large but they might've be generated them with a hash or something
            //TODO: See if we can discard handle + num completely and regenerate them on export. One problem is that changes to one zone might require regenerating all zones on the SP map (assuming there's cross zone object relations)
            return largestHandle + 1;
        }

        private u32 GetNewObjectNum()
        {
            //Find largest handle in use and add one
            u32 largestNum = 0;
            for (Zone zone in Map.Zones)
            {
                for (ZoneObject obj in zone.Objects)
                {
                    if (obj.Num > largestNum)
                    {
                        largestNum = obj.Num;
                    }
                }
            }

            //Error if Num is out of range
            if (largestNum == u32.MaxValue)
            {
                Logger.Error("Failed to find new object num. Returning 0xFFFFFFFF");
                return u32.MaxValue;
            }

            //TODO: See if we can start at 0 instead
            //TODO: See if we can discard handle + num completely and regenerate them on export. One problem is that changes to one zone might require regenerating all zones on the SP map (assuming there's cross zone object relations)
            return largestNum + 1;
        }

        private void OpenObjectCreationDialog(StringView initialObjectType = "object_dummy", ZoneObject parent = null)
        {
            CreateObjectDialog.Show(this.Map, new (dialogResult) =>
	        {
	            if (dialogResult == .Yes && CreateObjectDialog.PendingObject != null)
	            {
                    ZoneObject newObject = CreateObjectDialog.PendingObject;
                    NanoDB.AddObject(newObject);
                    Map.Zones[0].Objects.Add(newObject); //TODO: Will need to let user choose which zone to add it to when SP editing is added

                    newObject.Name.Set(CreateObjectDialog.[Friend]_nameBuffer);
                    newObject.Classname.Set(CreateObjectDialog.[Friend]_selectedType.Classname);
                    newObject.Handle = GetNewObjectHandle();
                    newObject.Num = GetNewObjectNum();
                    newObject.BBox.Min = .Zero;
                    newObject.BBox.Max = .(10.0f, 10.0f, 10.0f);
                    newObject.Position = newObject.BBox.Center();
                    if (CreateObjectDialog.Parent != null)
                    {
                        newObject.Parent = CreateObjectDialog.Parent;
                        CreateObjectDialog.Parent.Children.Add(newObject);
                        _expandObjectInOutliner = CreateObjectDialog.Parent;
                    }

                    CreateObjectDialog.Parent = null;
                    CreateObjectDialog.PendingObject = null;
                    _scrollToObjectInOutliner = newObject;
                    UnsavedChanges = true;
	            }
	        }, initialObjectType, parent);
        }

        private ZoneObject ShallowCloneObject(ZoneObject object)
        {
            ZoneObject clone = object.Clone();
            clone.Handle = GetNewObjectHandle();
            clone.Num = GetNewObjectNum();
            clone.Children.Clear();

            //Add RenderObject to scene so it gets drawn and destroyed when the scene is destroyed
            if (object.RenderObject != null)
            {
                clone.RenderObject = Scene.CloneRenderObject(object.RenderObject);
            }

            //Add to NanoDB
            NanoDB.AddObject(clone);

            //TODO: Will need to be updated to use the zone the original is in when SP editing is added. Probably should just a Zone field to ZoneObject
            //Add to zone
            Zone zone = Map.Zones[0];
            zone.Objects.Add(clone);

            UnsavedChanges = true;
            return clone;
        }

        private ZoneObject DeepCloneObject(ZoneObject object)
        {
            ZoneObject clone = ShallowCloneObject(object);
            for (ZoneObject child in object.Children)
            {
                ZoneObject childClone = DeepCloneObject(child);
                childClone.Parent = clone;
                clone.Children.Add(childClone);
            }

            UnsavedChanges = true;
            return clone;
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

[BonTarget, ReflectAll]
public class ZoneObjectClasses : EditorObject
{
    public List<ZoneObjectClass> Classes = new .() ~DeleteContainerAndItems!(_);
}