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
using System.Linq;

namespace Nanoforge.Rfg.Import
{
	public class MapImporter
	{
        //Import all assets used by an RFG territory
        public Result<Territory, StringView> ImportMap(StringView name)
        {
            gTaskDialog.Show(6);
            using (var changes = BeginCommit!(scope $"Import map - {name}"))
            {
                Territory map = changes.CreateObject<Territory>(name);
                map.PackfileName.Set(scope $"{name}.vpp_pc");

                //Preload all files in this map. Most important for str2_pc files since they require full unpack even for a single file
                gTaskDialog.SetStatus("Preloading ns_base.str2_pc");
                PackfileVFS.PreloadDirectory(scope $"//data/{map.PackfileName}/ns_base.str2_pc/");
                defer
                {
                    PackfileVFS.UnloadDirectory(scope $"//data/{map.PackfileName}/ns_base.str2_pc/");
                }
                gTaskDialog.Step();

                //Import zones
                gTaskDialog.SetStatus("Importing zones...");
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
                gTaskDialog.Step();
                Logger.Info("Done importing zones");

                //Import terrain, roads, and rocks
                Logger.Info("Importing terrain...");
                gTaskDialog.SetStatus("Importing terrain...");
                TerrainImporter terrainImporter = new .();
                defer delete terrainImporter;
                for (Zone zone in map.Zones)
                {
                    if (terrainImporter.LoadTerrain(map.PackfileName, map, zone, changes, name) case .Err)
                    {
                        Logger.Error("Failed to import terrain for zone '{}'", zone.Name);
                        //changes.Rollback();
                        return .Err("Failed to import terrain. Check the log.");
                    }    
                }
                gTaskDialog.Step();
                Logger.Info("Done importing terrain");

                //Import chunks
                gTaskDialog.SetStatus("Importing chunks...");
                for (Zone zone in map.Zones)
                {
                    for (ZoneObject obj in zone.Objects)
                    {
                        if (!obj.GetType().HasBaseType(typeof(ObjectMover)))
                            continue;

                        //Get chunk filename
                        ObjectMover mover = obj as ObjectMover;
                        String chunkFilename = scope .()..Append(mover.ChunkName.Enabled ? mover.ChunkName.Value : "UnknownChunkName")..Replace(".rfgchunk_pc", ".cchk_pc"); //Replace .rfgchunk_pc extension with the real one
                        String chunkName = Path.GetFileNameWithoutExtension(chunkFilename, .. scope .());

                        //See if chunk was already imported to prevent repeats
                        var search = map.Chunks.Select((chunk) => chunk).Where((chunk) => chunk.Name.Equals(chunkName));
                        if (search.Count() > 0)
                        {
                            mover.ChunkData = search.First(); //Already imported
                        }
                        else
                        {
                            //First time importing the chunk for this map
                            switch (ChunkImporter.LoadChunk(map.PackfileName, map, zone, changes, chunkName))
                            {
                                case .Ok(Chunk chunk):
                                    map.Chunks.Add(chunk);
                                    mover.ChunkData = chunk;
                                case .Err:
                                    Logger.Error("Failed to import chunk {}", chunkName);
                            }

                        }
                    }
                }
                gTaskDialog.Step();

                //Cache the contents of the vpp_pc so they're preserved if the original vpp_pc is edited
                String importCacheFolder = scope $"{NanoDB.CurrentProject.Directory}Import\\{map.PackfileName}\\";
                Directory.CreateDirectory(importCacheFolder);
                gTaskDialog.SetStatus("Caching map files...");
                for (var vppFile in PackfileVFS.Enumerate(scope $"//data/{map.PackfileName}/"))
                {
                    if (vppFile.IsFile) //File in the root of the vpp_pc (not a str2_pc. those are directories)
                    {
                        switch (PackfileVFS.ReadAllBytes(vppFile as PackfileVFS.FileEntry))
                        {
                            case .Ok(u8[] bytes):
                                defer delete bytes;
                                if (File.WriteAll(scope $"{importCacheFolder}{vppFile.Name}", bytes) case .Err)
                                {
                                    Logger.Error("Failed to write {} to import cache for {}", vppFile.Name, map.PackfileName);
                                    return .Err("Failed to write imported files to cache.");
                                }
                            case .Err:
                                Logger.Error("Failed to extract {} for import cache for {}", vppFile.Name, map.PackfileName);
                                return .Err("Failed to extract imported files for cache.");
                        }
                    }
                    else if (vppFile.IsDirectory) //str2_pc file
                    {
                        String str2NameNoExt = Path.GetFileNameWithoutExtension(vppFile.Name, .. scope .());
                        String str2Directory = scope $"{importCacheFolder}{str2NameNoExt}\\";
                        Directory.CreateDirectory(str2Directory);
                        for (var str2File in (vppFile as PackfileVFS.DirectoryEntry).Entries) //Files inside the str2_pc
                        {
                            if (!str2File.IsFile)
                                continue;

                            switch (PackfileVFS.ReadAllBytes(str2File as PackfileVFS.FileEntry))
                            {
                                case .Ok(u8[] bytes):
                                    defer delete bytes;
                                    if (File.WriteAll(scope $"{str2Directory}{str2File.Name}", bytes) case .Err)
                                    {
                                        Logger.Error("Failed to write {}\\{} to import cache for {}", vppFile.Name, str2File.Name, map.PackfileName);
                                        return .Err("Failed to write imported files to cache.");
                                    }
                                case .Err:
                                    Logger.Error("Failed to extract {}\\{} for import cache for {}", vppFile.Name, str2File.Name, map.PackfileName);
                                    return .Err("Failed to extract imported files for cache.");
                            }
                        }
                    }
                }
                gTaskDialog.Step();

                //Load additional map data if present
                if (PackfileVFS.Exists(scope $"//data/{map.PackfileName}/EditorData.xml"))
                {
                    //TODO: Implement
                }

                gTaskDialog.SetStatus("Done!");
                gTaskDialog.Step();
                gTaskDialog.Close();

                return .Ok(map);
            }
        }

        public Result<Zone, StringView> ImportZone(StringView packfileName, StringView filename, DiffUtil changes)
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

            ZoneImporter zoneImporter = new .();
            defer delete zoneImporter;
            switch (zoneImporter.ImportZone(zoneBytes.Value, persistentZoneBytes.Value, filename, changes))
            {
                case .Ok(let zone):
                    return .Ok(zone);
                case .Err(let err):
                    return .Err(err);
            }
        }
	}
}