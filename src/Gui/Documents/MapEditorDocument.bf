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

namespace Nanoforge.Gui.Documents
{
	public class MapEditorDocument : GuiDocumentBase
	{
        public readonly append String MapName;
        public bool Loading { get; private set; } = true;
        private bool _loadFailure = false;
        private StringView _loadFailureReason = .();
        public Territory Map = null;
        private Scene _scene = null;
        private ZoneObject _selectedObject;
        private Zone _selectedZone = null;

        //Deepest tree level the outliner will draw. Need to limit to prevent accidental stack overflow
        private const int MAX_OUTLINER_DEPTH = 7;

        public this(StringView mapName)
        {
            MapName.Set(mapName);
            HasMenuBar = false;
            NoWindowPadding = true;
            HasCustomOutlinerAndInspector = true;
        }

        private void Load(App app)
        {
            Loading = true;
            defer { Loading = false; }

            //Create scene for rendering
            Renderer renderer = app.GetResource<Renderer>();
            _scene = renderer.CreateScene();

            //Check if the map was already imported
            Territory findResult = ProjectDB.Find<Territory>(MapName);
            if (findResult != null)
            {
                Map = findResult;
                Map.Load();
                return;
            }

            //Map needs to be imported
            switch (MapImporter.ImportMap(MapName))
            {
                case .Ok(var newMap):
            		Map = newMap;
                    Map.Load();
                    Logger.Info("Finished importing map '{}'", MapName);
                case .Err(StringView err):
            		_loadFailure = true;
                    _loadFailureReason = err;
                    Logger.Error("Failed to import map '{}'. {}. See the log for more details.", MapName, err);
                    return;
            }

            //Create render objects for meshes
            for (Zone zone in Map.Zones)
            {
                for (int i in 0 ... 8)
                {
                    List<u8> indexBuffer = null;
                    List<u8> vertexBuffer = null;
                    defer { DeleteIfSet!(indexBuffer); }
                    defer { DeleteIfSet!(vertexBuffer); }
                    if (zone.LowLodTerrainIndexBuffers[i].Load() case .Ok(let val))
                    {
						indexBuffer = val;
                    }
                    else
                    {
                        Logger.Error("Failed to load index buffer for low lod terrain submesh {} of {}.", i, zone.Name);
                        continue;
                    }
                    if (zone.LowLodTerrainVertexBuffers[i].Load() case .Ok(let val))
                    {
                    	vertexBuffer = val;
                    }
                    else
                    {
                        Logger.Error("Failed to load vertex buffer for low lod terrain submesh {} of {}.", i, zone.Name);
                        continue;
                    }

                    Mesh mesh = new .();
                    if (mesh.Init(renderer.Device, zone.LowLodTerrainMeshConfig[i], indexBuffer, vertexBuffer) case .Ok)
                    {
                        if (_scene.CreateRenderObject("TerrainLowLod", mesh, zone.TerrainPosition, .Identity) case .Err)
                        {
                            Logger.Error("Failed to create render object for low lod terrain submesh {} of {}", i, zone.Name);
                            continue;
                        }
                    }
                    else
                    {
                        Logger.Error("Failed to create render object for low lod terrain submesh {} of {}", i, zone.Name);
                        delete mesh;
                    }
                }
            }
        }

        public override void Update(App app, Gui gui)
        {
            if (FirstDraw)
            {
                ThreadPool.QueueUserWorkItem(new () => { this.Load(app); });
            }

            if (_scene != null)
            {
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
            if (Loading)
                return;

            //Draw object bounding boxes
            Random rand = scope .(0);
            for (Zone zone in Map.Zones)
            {
                for (ZoneObject obj in zone.Objects)
                {
                    Vec4<f32> color = .((f32)(rand.NextI32() % 255) / 255.0f, (f32)(rand.NextI32() % 255) / 255.0f, (f32)(rand.NextI32() % 255) / 255.0f, 1.0f);
                    color *= 1.9f;
                    _scene.DrawBox(obj.BBox.Min, obj.BBox.Max, color);
                }
            }
        }

        public override void Save(App app, Gui gui)
        {
            return;
        }

        public override void OnClose(App app, Gui gui)
        {
            if (_scene != null)
            {
                Renderer renderer = app.GetResource<Renderer>();
                renderer.DestroyScene(_scene);
            }
            
            return;
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

            //Set custom highlight colors for the table
            Vec4<f32> selectedColor = .(0.157f, 0.350f, 0.588f, 1.0f);
            Vec4<f32> highlightedColor = selectedColor * 1.1f;
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

        private bool Outliner_ZoneAnyChildObjectsVisible(Zone zone)
        {
            //TODO: Implement
            return true;
        }

        private bool Outliner_ShowObjectOrChildren(ZoneObject obj)
        {
            //TODO: Implement
            return true;
        }

        private void Outliner_DrawObjectNode(ZoneObject obj, int depth)
        {
            if (depth > MAX_OUTLINER_DEPTH)
            {
                ImGui.Text("Can't draw object! Hit outliner depth limit");
                return;
            }

            bool show = Outliner_ShowObjectOrChildren(obj);
            if (!show)
                return;

            bool selected = (obj == _selectedObject);
            String label = scope .();
            if (obj.GetName() case .Ok(let name))
                label.Set(name);
            else
                label.Set(obj.Classname);

            ImGui.TableNextRow();
            ImGui.TableNextColumn();

            bool hasChildren = obj.Children.Count > 0;
            bool hasParent = obj.Parent != null;

            //Draw node
            ImGui.PushID(Internal.UnsafeCastToPtr(obj));
            f32 nodeXPos = ImGui.GetCursorPosX();
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
                ImGui.TooltipOnPrevious(obj.Classname);
            }

            //Flags
            ImGui.TableNextColumn();
            ImGui.Text(obj.Flags.ToString(.. scope .()));

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

        }
	}
}