using System;
using System.IO;
using System.Threading;
using Nanoforge.FileSystem;
using Nanoforge.Gui.ViewModels.Dialogs;
using Nanoforge.Gui.Views;
using Nanoforge.Gui.Views.Dialogs;
using Serilog;

namespace Nanoforge.Rfg.Import;

public class MapImporter
{
    public bool Loading = false;
    public bool Success = false;
    
    public void ImportMapInBackground(string name)
    {
        TaskDialog dialog = new TaskDialog();
        dialog.ShowDialog(MainWindow.Instance);
        ThreadPool.QueueUserWorkItem(_ => ImportMap(name, dialog.ViewModel));
    }
    
    //Import all assets used by an RFG territory into the current project
    private void ImportMap(string name, TaskDialogViewModel? status = null)
    {
        string packfilePath = $"//data/{name}/";
        try
        {
            Loading = true;
            Success = false;
            status?.Setup(6, $"Importing map {name}...");
            Log.Information($"Importing map {name}...");

            //EditorObject is created, but it is not added to NanoDB until the map has been successfully imported.
            Territory map = new() { Name = name };
            map.PackfileName = $"{name}";

            //Get packfile data format so it can be preserved on export
            EntryBase? mapPackfile = PackfileVFS.GetEntry($"//data/{map.PackfileName}/");
            if (mapPackfile == null)
            {
                throw new Exception($"Failed to get VFS entry for map packfile. {map.PackfileName} not found.");
            }
            if (!mapPackfile.IsDirectory)
            {
                throw new Exception($"Failed to get VFS entry for map packfile. {map.PackfileName} is not a DirectoryEntry...");
            }
            map.Compressed = (mapPackfile as DirectoryEntry)!.Compressed;
            map.Condensed = (mapPackfile as DirectoryEntry)!.Condensed;
            
            //Preload all files in this map. Most important for str2_pc files since they require full unpack even for a single file
            status?.SetStatus("Preloading files...");
            PackfileVFS.PreloadDirectory(packfilePath, recursive: true);
            status?.NextStep();
            
            //Import the rfgzone_pc files
            status?.SetStatus("Importing zones...");
            foreach (var entry in PackfileVFS.Enumerate($"//data/{map.PackfileName}/"))
            {
                if (Path.GetExtension(entry.Name) != ".rfgzone_pc" || entry.Name.StartsWith("p_", StringComparison.OrdinalIgnoreCase))
                    continue;

                if (!ImportZone(map.PackfileName, entry.Name, out Zone? zone) || zone is null)
                {
                    Log.Error($"Failed to import zone '{entry.Name}'. Cancelling map import.");
                    status?.SetStatus($"Failed to import zone '{entry.Name}'. Check the log.");
                    return;
                }

                map.Zones.Add(zone);
            }
            status?.NextStep();
            Log.Information("Done importing zones");
            
            //Import terrain, roads, and rocks
            Log.Information("Importing terrain...");
            status?.SetStatus("Importing terrain...");
            //TODO: Port
            status?.NextStep();
            Log.Information("Done importing terrain");
            
            //Import chunks
            status?.SetStatus("Importing chunks...");
            //TODO: Port
            status?.NextStep();
            
            //Cache the contents of the vpp_pc so they're preserved if the original vpp_pc is edited
            status?.SetStatus("Caching map files...");
            //TODO: Port
            status?.NextStep();
            
            //Load additional map data if present
            //TODO: Port code to load EditorData.xml
            
            status?.SetStatus("Done!");
            status?.NextStep();
            status?.CloseDialog();
            
            //TODO: Pass a delegate that gets called when the import is complete (whether it fails or not). Use that to open a new document for the freshly imported map.
            //TODO: Load all the created objects into NanoDB. Likely will want to create changes/diff helper class like in the old version to track all new objects and help create them
            
            Success = true;
        }
        catch (Exception ex)
        {
            Log.Error(ex, $"Error importing map: {name}");
            status?.SetStatus("Import failed. Check the log");
            Success = false;
        }
        finally
        {
            Loading = false;
            if (status != null)
            {
                status.CanClose = true;
            }
            PackfileVFS.UnloadDirectory(packfilePath, recursive: true);
        }
    }

    private bool ImportZone(string packfileName, string zoneFileName, out Zone? zone)
    {
        zone = null;
        try
        {
            string zoneFilePath = $"//data/{packfileName}/{zoneFileName}";
            Stream? zoneFile = PackfileVFS.OpenFile(zoneFilePath);
            if (zoneFile == null)
            {
                Log.Error($"Failed to open {zoneFilePath} from VFS.");
                return false;
            }
            
            string persistentZoneFilePath = $"//data/{packfileName}/p_{zoneFileName}";
            Stream? persistentZoneFile = PackfileVFS.OpenFile(persistentZoneFilePath);
            if (persistentZoneFile == null)
            {
                Log.Error($"Failed to open {persistentZoneFilePath} from VFS.");
                return false;
            }

            ZoneImporter importer = new();
            zone = importer.ImportZone(zoneFile, persistentZoneFile, zoneFileName);
            return zone is not null;
        }
        catch (Exception ex)
        {
            Log.Error(ex, $"Error importing zone file: {zoneFileName}");
            zone = null;
            return false;
        }
    }
}