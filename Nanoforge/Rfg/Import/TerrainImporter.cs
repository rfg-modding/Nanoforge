using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using Nanoforge.Editor;
using Nanoforge.FileSystem;
using RFGM.Formats.Meshes;
using RFGM.Formats.Meshes.Shared;
using RFGM.Formats.Peg;
using RFGM.Formats.Peg.Models;
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

            //Index textures in this maps packfile + common file so they can be found
            TextureIndex.IndexVpp(packfileName);
            TextureIndex.IndexVpp("mp_common.vpp_pc"); //TODO: Do this based on precache file name instead of hardcoding it

            //Load zone-wide textures
            {
                ProjectTexture? combTexture = GetOrLoadTerrainTexture($"{name}comb.tga", createdObjects);
                ProjectTexture? ovlTexture = GetOrLoadTerrainTexture($"{name}_ovl.tga", createdObjects);
                ProjectTexture? splatmap = GetOrLoadTerrainTexture($"{name}_alpha00.tga", createdObjects);
                if (combTexture == null)
                    Log.Warning("Failed to load comb texture for {}", name);
                if (ovlTexture == null)
                    Log.Warning("Failed to load ovl texture for {}", name);
                if (splatmap == null)
                    Log.Warning("Failed to load splatmap texture for {}", name);

                terrain.CombTexture = combTexture;
                terrain.OvlTexture = ovlTexture;
                terrain.Splatmap = splatmap;
            }

            //TODO: Port code to load high lods meshes, rock meshes, road meshes, and their textures. This time split each into it's own function.
            //TODO: Start at line 96 of TerrainImporter.bf

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

    private ProjectTexture? GetOrLoadTerrainTexture(string tgaName, List<EditorObject> createdObjects)
    {
        try
        {
            ImportedTextures importedTextures = NanoDB.FindOrCreate<ImportedTextures>("Global::ImportedTextures");
            if (importedTextures.GetTexture(tgaName) is { } cachedTexture)
                return cachedTexture;

            //Get file paths
            string pegCpuFilePath = TextureIndex.GetTexturePegPath(tgaName) ?? throw new Exception($"Importer failed to find peg path for texture {tgaName} during terrain import");
            string pegGpuFilePath = new string(pegCpuFilePath);
            if (pegGpuFilePath.EndsWith(".cpeg_pc"))
                pegGpuFilePath = pegGpuFilePath.Replace(".cpeg_pc", ".gpeg_pc");
            if (pegGpuFilePath.EndsWith(".cvbm_pc"))
                pegGpuFilePath = pegGpuFilePath.Replace(".cvbm_pc", ".gvbm_pc");

            //Extract cpu file & gpu file
            Stream cpuFile = PackfileVFS.OpenFile(pegCpuFilePath) ?? throw new Exception($"Importer failed to open peg cpu file at {pegCpuFilePath}");
            Stream gpuFile = PackfileVFS.OpenFile(pegGpuFilePath) ?? throw new Exception($"Importer failed to open peg gpu file at {pegGpuFilePath}");

            PegReader reader = new();
            ProjectTexture? result = null;
            LogicalTextureArchive peg = reader.Read(cpuFile, gpuFile, tgaName, CancellationToken.None);
            foreach (LogicalTexture logicalTexture in peg.LogicalTextures)
            {
                using var memoryStream = new MemoryStream();
                logicalTexture.Data.CopyTo(memoryStream);
                byte[] pixels = memoryStream.ToArray();

                Silk.NET.Vulkan.Format pixelFormat = ProjectTexture.PegFormatToVulkanFormat(logicalTexture.Format, logicalTexture.Flags);
                ProjectBuffer pixelBuffer = NanoDB.CreateBuffer(pixels, logicalTexture.Name) ?? throw new Exception($"Importer failed to create buffer for {logicalTexture.Name}");
                ProjectTexture texture = new(pixelBuffer)
                {
                    Format = pixelFormat,
                    Width = logicalTexture.Size.Width,
                    Height = logicalTexture.Size.Height,
                    NumMipLevels = logicalTexture.MipLevels,
                };

                if (string.Equals(logicalTexture.Name, tgaName, StringComparison.OrdinalIgnoreCase))
                {
                    result = texture;
                }

                //Track imported textures so we don't import multiple times
                importedTextures.AddTexture(texture);
            }

            if (result == null)
            {
                Log.Error($"TerrainImporter.GetOrLoadTerrainTexture() failed to find '{tgaName}'");
            }

            return result;
        }
        catch (Exception ex)
        {
            Log.Error(ex, $"Failed to load terrain texture {tgaName}");
            return null;
        }
    }
}