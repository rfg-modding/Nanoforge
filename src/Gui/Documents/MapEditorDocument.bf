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

            //Create scene for rendering
            Renderer renderer = app.GetResource<Renderer>();
            _scene = renderer.CreateScene();
        }

        public override void Update(App app, Gui gui)
        {
            if (FirstDraw)
            {
                ThreadPool.QueueUserWorkItem(new () => { this.Load(app); });
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

        public override void Save(App app, Gui gui)
        {
            return;
        }

        public override void OnClose(App app, Gui gui)
        {
            return;
        }

        public override bool CanClose(App app, Gui gui)
        {
            return true;
        }

        public override void Outliner(App app, Gui gui)
        {

        }

        public override void Inspector(App app, Gui gui)
        {

        }
	}
}