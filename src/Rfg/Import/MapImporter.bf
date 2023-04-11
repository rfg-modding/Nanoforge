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

                //Find primary packfile
                var packfileSearch = PackfileVFS.GetPackfile(map.PackfileName);
                if (packfileSearch case .Err(let err))
                {
                    Logger.Error(scope $"Map importer failed to locate primary packfile '{map.PackfileName}'");
                    changes.Rollback();
					return .Err("Failed to find the maps primary packfile. Check the log.");
				}

                //Import zones
                Logger.Info("Importing zones...");
                PackfileV3 packfile = packfileSearch.Value;
                for (int i in 0..<packfile.Entries.Count)
                {
                    StringView entryName = packfile.EntryNames[i];
                    if (Path.GetExtension(entryName, .. scope .()) != ".rfgzone_pc" || entryName.StartsWith("p_", .OrdinalIgnoreCase))
                        continue;

                    switch (ImportZone(packfile, entryName, changes))
                    {
                        case .Ok(let zone):
                            map.Zones.Add(zone);
                        case .Err(let err):
                            Logger.Error(scope $"Failed to import zone '{entryName}' for '{name}'. {err}");
                            changes.Rollback();
                            return .Err("Failed to import a zone. Check the log.");
                    }
                }
                Logger.Info("Done importing zones");

                //Import terrain, roads, and rocks
                Logger.Info("Importing terrain...");
                for (Zone zone in map.Zones)
                {
                    if (TerrainImporter.LoadTerrain(packfile, zone, changes, name) case .Err)
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
                if (packfile.Contains("EditorData.xml"))
                {
                    //TODO: Implement
                }

                return .Ok(map);
            }
        }

        public static Result<Zone, StringView> ImportZone(PackfileV3 packfile, StringView filename, DiffUtil changes)
        {
            var zoneBytes = packfile.ReadSingleFile(filename);
            if (zoneBytes case .Err(let err))
            {
                Logger.Error("Failed to extract '{}'. {}", filename, err);
                return .Err("Failed to extract zone file bytes");
            }
            defer delete zoneBytes.Value;

            var persistentZoneBytes = packfile.ReadSingleFile(scope $"p_{filename}");
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