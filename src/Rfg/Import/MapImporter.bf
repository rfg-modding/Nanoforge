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

                //Import terrain, roads, and rocks
                for (Zone zone in map.Zones)
                {
                    if (LoadTerrain(packfile, zone, changes, name) case .Err)
                    {
                        Logger.Error("Failed to import terrain for zone '{}'", zone.Name);
                        //TODO: RE-ENABLE AND MAKE SURE IT WORKS BEFORE COMMIT
                        //changes.Rollback();
                        return .Err("Failed to import terrain. Check the log.");
                    }    
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

        public static Result<void> LoadTerrain(PackfileV3 packfile, Zone zone, DiffUtil changes, StringView name)
        {
            //Determine terrain position from obj_zone center
            for (ZoneObject obj in zone.Objects)
            {
                if (obj.Classname == "obj_zone")
                {
                    zone.TerrainPosition = obj.BBox.Center();
                    break;
                }
            }

            //Load ns_base.str2_pc. Contains the terrain meshes. This will need to be changed when SP support is added since SP vpp structure is more complex
            var str2ReadResult = packfile.ReadSingleFile("ns_base.str2_pc");
            if (str2ReadResult case .Err(StringView err))
            {
                Logger.Error("Failed to load ns_base.str2_pc for terrain import. Error: {}", err);
                return .Err;
            }

            //Parse ns_base.str2_pc
            defer delete str2ReadResult.Value;
            PackfileV3 nsBase = scope .(new ByteSpanStream(str2ReadResult.Value), "ns_base.str2_pc");
            if (nsBase.ReadMetadata() case .Err(StringView err))
            {
                Logger.Info("Failed to parse ns_base.str2_pc for terrain import. Error: {}", err);
                return .Err;
            }

            //Get cterrain_pc and gterrain_pc files
            u8[] cpuFile = null;
            u8[] gpuFile = null;
            defer { DeleteIfSet!(cpuFile); }
            defer { DeleteIfSet!(gpuFile); }

            switch (nsBase.ReadSingleFile(scope $"{name}.cterrain_pc"))
            {
                case .Ok(u8[] bytes):
                    cpuFile = bytes;
                case .Err(StringView err):
                    Logger.Error("Failed to extract cterrain_pc file. Error: {}", err);
                    return .Err;
            }
            switch (nsBase.ReadSingleFile(scope $"{name}.gterrain_pc"))
            {
                case .Ok(u8[] bytes):
                    gpuFile = bytes;
                case .Err(StringView err):
                    Logger.Error("Failed to extract gterrain_pc file. Error: {}", err);
                    return .Err;
            }

            Terrain terrain = scope .();
            if (terrain.Load(cpuFile, gpuFile, false) case .Err(StringView err))
            {
                Logger.Error("Failed to parse cterrain_pc file. Error: {}", err);
                return .Err;
            }

            for (int i in 0 ... 8)
            {
                switch (terrain.GetMeshData(i))
                {
                    case .Ok(MeshInstanceData meshData):
                        ProjectBuffer indexBuffer = ProjectDB.CreateBuffer(meshData.IndexBuffer, scope $"{name}_low_lod_{i}");
                        ProjectBuffer vertexBuffer = ProjectDB.CreateBuffer(meshData.VertexBuffer, scope $"{name}_low_lod_{i}");
                        zone.LowLodTerrainMeshConfig[i] = meshData.Config.Clone(.. new .());
                        zone.LowLodTerrainIndexBuffers[i] = indexBuffer;
                        zone.LowLodTerrainVertexBuffers[i] = vertexBuffer;
                    case .Err(StringView err):
                        Logger.Error("Failed to get mesh data from gterrain_pc file. Error: {}", err);
                        return .Err;
                }
            }

            //Load subzones (ctmesh_pc|gtmesh_pc files). These have the high lod terrain meshes
            for (int i in 0 ... 8)
            {
                u8[] subzoneCpuFile = null;
                u8[] subzoneGpuFile = null;
                defer { DeleteIfSet!(subzoneCpuFile); }
                defer { DeleteIfSet!(subzoneGpuFile); }

                switch (nsBase.ReadSingleFile(scope $"{name}_{i}.ctmesh_pc"))
                {
                    case .Ok(u8[] bytes):
                        subzoneCpuFile = bytes;
                    case .Err(StringView err):
                        Logger.Error("Failed to extract terrain subzone {}_{}.ctmesh_pc. Error: {}", name, i, err);
                        return .Err;
                }
                switch (nsBase.ReadSingleFile(scope $"{name}_{i}.gtmesh_pc"))
                {
                    case .Ok(u8[] bytes):
                        subzoneGpuFile = bytes;
                    case .Err(StringView err):
                        Logger.Error("Failed to extract terrain subzone {}_{}.gtmesh_pc. Error: {}", name, i, err);
                        return .Err;
                }

                TerrainSubzone subzone = scope .();
                if (subzone.Load(subzoneCpuFile, subzoneGpuFile, false) case .Err(StringView err))
                {
                    Logger.Error("Failed to parse {}_{}.ctmesh_pc. Error: {}", name, i, err);
                    return .Err;
                }

                //Load terrain mesh
                switch (subzone.GetTerrainMeshData())
                {
                    case .Ok(MeshInstanceData meshData):
                        ProjectBuffer indexBuffer = ProjectDB.CreateBuffer(meshData.IndexBuffer, scope $"{name}_high_lod_{i}");
                        ProjectBuffer vertexBuffer = ProjectDB.CreateBuffer(meshData.VertexBuffer, scope $"{name}_high_lod_{i}");
                        zone.HighLodTerrainMeshConfig[i] = meshData.Config.Clone(.. new .());
                        zone.HighLodTerrainIndexBuffers[i] = indexBuffer;
                        zone.HighLodTerrainVertexBuffers[i] = vertexBuffer;
                        zone.HighLodTerrainMeshPositions[i] = subzone.Data.Position;
                    case .Err(StringView err):
                        Logger.Error("Failed to get high lod terrain mesh data from {}_{}.gtmesh_pc. Error: {}", name, i, err);
                        return .Err;
                }
            }

            return .Ok;
        }
	}
}