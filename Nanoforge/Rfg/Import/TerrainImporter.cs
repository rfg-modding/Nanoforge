using System;
using System.Collections.Generic;
using System.IO;
using Nanoforge.Editor;
using Nanoforge.FileSystem;
using RFGM.Formats.Meshes;
using RFGM.Formats.Meshes.Shared;
using Serilog;

namespace Nanoforge.Rfg.Import;

public class TerrainImporter
{
    public ZoneTerrain? ImportTerrain(string packfileName, string name, Territory territory, Zone zone, List<EditorObject> createdObjects)
    {
        try
        {
            ZoneTerrain terrain = new() { Name = $"{name}.cterrain_pc" };
            createdObjects.Add(terrain);

            //Determine terrain position from obj_zone center
            foreach (ZoneObject obj in zone.Objects)
            {
                if (obj.GetType() == typeof(ObjZone))
                {
                    terrain.Position = obj.BBox.Center();
                    break;
                }
            }

            Log.Information($"Importing primary terrain files for {zone.Name}...");
            LoadPrimaryTerrain(packfileName, name, terrain, createdObjects);
            Log.Information("Done importing primary terrain files.");

            //TODO: Port code to load high lods meshes, rock meshes, road meshes, and their textures. This time split each into it's own function.
            //TODO: Start at line 75 of TerrainImporter.bf

            return terrain;
        }
        catch (Exception ex)
        {
            Log.Error(ex, $"Error importing terrain for {packfileName}");
            return null;
        }
    }

    //Load .cterrain_pc and .gterrain_pc files. There's one of each per zone. They contain info about the overall zones terrain and the low lod terrain meshes.
    private void LoadPrimaryTerrain(string packfileName, string name, ZoneTerrain terrain, List<EditorObject> createdObjects)
    {
        string cpuFilePath = $"//data/{packfileName}/ns_base.str2_pc/{name}.cterrain_pc";
        string gpuFilePath = $"//data/{packfileName}/ns_base.str2_pc/{name}.gterrain_pc";

        //Read header file (.cterrain_pc)
        Stream cpuFile = PackfileVFS.OpenFile(cpuFilePath) ?? throw new Exception($"Importer failed to load .cterrain_pc file from {cpuFilePath}");
        Stream gpuFile = PackfileVFS.OpenFile(gpuFilePath) ?? throw new Exception($"Importer failed to load .gterrain_pc file from {gpuFilePath}");
        TerrainFile terrainFile = new(name);
        terrainFile.ReadHeader(cpuFile);

        //Save low lod meshes to buffers
        for (int i = 0; i < 9; i++)
        {
            MeshInstanceData meshData = terrainFile.ReadData(gpuFile, i);
            ProjectBuffer indexBuffer = NanoDB.CreateBuffer(meshData.Indices, $"{name}_low_lod_{i}_indices") ??
                                        throw new Exception($"Failed to create buffer for indices of mesh {i} of low lod terrain for {name}");
            ProjectBuffer vertexBuffer = NanoDB.CreateBuffer(meshData.Vertices, $"{name}_low_lod_{i}_vertices") ??
                                         throw new Exception($"Failed to create buffer for vertices of mesh {i} of low lod terrain for {name}");
            ProjectMesh mesh = new() { Name = $"{name}_low_lod_{i}_mesh" };
            mesh.InitFromRfgMeshConfig(meshData.Config);
            mesh.IndexBuffer = indexBuffer;
            mesh.VertexBuffer = vertexBuffer;
            terrain.LowLodTerrainMeshes[i] = mesh;
            createdObjects.Add(mesh);
        }
    }
}