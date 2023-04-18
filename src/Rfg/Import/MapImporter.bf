using Nanoforge.App.Project;
using Nanoforge.FileSystem;
using RfgTools.Formats;
using Nanoforge.Misc;
using Nanoforge.App;
using System.IO;
using Common;
using System;
using Common.IO;
using RfgTools.Formats.Meshes;
using Nanoforge.Render.Resources;
using Nanoforge.Render;
using RfgTools.Formats.Textures;
using System.Collections;

namespace Nanoforge.Rfg.Import
{
	public static class MapImporter
	{
        //Import all assets used by an RFG territory
        public static Result<Territory, StringView> ImportMap(StringView name)
        {
            using (var changes = BeginCommit!(scope $"Import map - {name}"))
            {
                Territory map = changes.CreateObject<Territory>(name);
                map.PackfileName.Set(scope $"{name}.vpp_pc");

                //Preload all files in this map. Most important for str2_pc files since they require full unpack even for a single file
                PackfileVFS.PreloadDirectory(scope $"//data/{map.PackfileName}/ns_base.str2_pc/");
                defer PackfileVFS.UnloadDirectory(scope $"//data/{map.PackfileName}/ns_base.str2_pc/");

                //Import zones
                Logger.Info("Importing zones...");
                for (var entry in PackfileVFS.Enumerate(scope $"//data/{map.PackfileName}/"))
                {
                    if (Path.GetExtension(entry.Name, .. scope .()) != ".rfgzone_pc" || entry.Name.StartsWith("p_", .OrdinalIgnoreCase))
	                    continue;

                    switch (ImportZone(map.PackfileName, entry.Name, changes))
                    {
                        case .Ok(let zone):
                            map.Zones.Add(zone);
                        case .Err(let err):
                            Logger.Error(scope $"Failed to import zone '{entry.Name}' for '{name}'. {err}");
                            //changes.Rollback();
                            return .Err("Failed to import a zone. Check the log.");
                    }
                }
                Logger.Info("Done importing zones");

                //Import terrain, roads, and rocks
                Logger.Info("Importing terrain...");
                for (Zone zone in map.Zones)
                {
                    if (TerrainImporter.LoadTerrain(map.PackfileName, zone, changes, name) case .Err)
                    {
                        Logger.Error("Failed to import terrain for zone '{}'", zone.Name);
                        changes.Rollback();
                        return .Err("Failed to import terrain. Check the log.");
                    }    
                }
                Logger.Info("Done importing terrain");

                //Import chunks
                for (Zone zone in map.Zones)
                {
                    //TODO: Implement
                }

                //Load additional map data if present
                if (PackfileVFS.Exists(scope $"//data/{map.PackfileName}/EditorData.xml"))
                {
                    //TODO: Implement
                }

                return .Ok(map);
            }
        }

        public static Result<Zone, StringView> ImportZone(StringView packfileName, StringView filename, DiffUtil changes)
        {
            Result<u8[]> zoneBytes = PackfileVFS.ReadAllBytes(scope $"//data/{packfileName}/{filename}");
            if (zoneBytes case .Err(let err))
            {
                Logger.Error("Failed to extract '{}'. {}", filename, err);
                return .Err("Failed to extract zone file bytes");
            }
            defer delete zoneBytes.Value;

            Result<u8[]> persistentZoneBytes = PackfileVFS.ReadAllBytes(scope $"//data/{packfileName}/p_{filename}");
            if (persistentZoneBytes case .Err(let err))
            {
                Logger.Error("Failed to extract '{}'. {}", filename, err);
                return .Err("Failed to extract persistent zone file bytes");
            }
            defer delete persistentZoneBytes.Value;

            switch (ZoneImporter.ImportZone(zoneBytes.Value, persistentZoneBytes.Value, filename, changes))
            {
                case .Ok(let zone):
                    return .Ok(zone);
                case .Err(let err):
                    return .Err(err);
            }
        }
	}
}