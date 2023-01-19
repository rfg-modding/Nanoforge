using System.Threading;
using Nanoforge.App;
using Nanoforge.Rfg;
using Nanoforge;
using System;
using ImGui;
using Nanoforge.Rfg.Import;
using Nanoforge.Misc;

namespace Nanoforge.Gui.Documents
{
	public class MapEditorDocument : GuiDocumentBase
	{
        public readonly append String MapName;
        public bool Loading { get; private set; } = true;
        private bool _loadFailure = false;
        private StringView _loadFailureReason = .();
        public Territory Map = null;

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
                    Log.Info("Finished importing map '{}'", MapName);
                case .Err(StringView err):
            		_loadFailure = true;
                    _loadFailureReason = err;
                    Log.Error("Failed to import map '{}'. {}. See the log for more details.", MapName, err);
                    return;
            }
        }

        public override void Update(App app, Gui gui)
        {
            if (FirstDraw)
            {
                ThreadPool.QueueUserWorkItem(new () => { this.Load(app); });
            }
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