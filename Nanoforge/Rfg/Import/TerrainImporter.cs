using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Threading;
using Nanoforge.Editor;
using Nanoforge.FileSystem;
using RFGM.Formats.Materials;
using RFGM.Formats.Meshes;
using RFGM.Formats.Meshes.Shared;
using RFGM.Formats.Meshes.Terrain;
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
            LoadPrimaryTerrain(packfileName, name, terrain, createdObjects, out TerrainFile terrainFile);
            Log.Information("Done importing primary terrain files.");

            //Index textures in this maps packfile + common file so they can be found
            if (!TextureIndex.IsIndexed(packfileName))
            {
                TextureIndex.IndexVpp(packfileName);
            }
            if (!TextureIndex.IsIndexed("mp_common.vpp_pc"))
            {
                TextureIndex.IndexVpp("mp_common.vpp_pc");
            }

            //Load zone-wide textures
            {
                //TODO: Drive these from the materials in the terrain files instead of hardcoding them
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
                
                //Load subzone textures
                RfgMaterial? blendMaterial = terrainFile.Materials.FirstOrDefault(mat => mat.ShaderName == "rl_terrain_height_mesh_blend");
                if (blendMaterial != null)
                {
                    Debug.Assert(blendMaterial.Textures.Count == 10); //This is the case for all vanilla cterrain_pc files
                    List<string> texturesNames = blendMaterial.Textures.Select(tex => tex.Name).ToList();
                    for (int textureIndex = 0; textureIndex < 8; textureIndex++) //This ends with the alpha and comb texture which the map loader handles setting
                    {
                        ProjectTexture? texture = GetOrLoadTerrainTexture(texturesNames[textureIndex], createdObjects);
                        terrain.SplatmapTextures[textureIndex] = texture;
                    }
                }
                else
                {
                    Log.Error($"Failed to find rl_terrain_height_mesh_blend material in {packfileName}. Terrain won't have textures.");
                }
            }

            //Load subzones (ctmesh_pc|gtmesh_pc files). These have the high lod terrain meshes and road meshes
            for (int subzoneIndex = 0; subzoneIndex < 9; subzoneIndex++)
            {
                LoadSubzones(packfileName, name, territory, terrain, subzoneIndex, createdObjects, terrainFile);
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
    private void LoadPrimaryTerrain(string packfileName, string name, ZoneTerrain terrain, List<EditorObject> createdObjects, out TerrainFile terrainFile)
    {
        string cpuFilePath = $"//data/{packfileName}/ns_base.str2_pc/{name}.cterrain_pc";
        string gpuFilePath = $"//data/{packfileName}/ns_base.str2_pc/{name}.gterrain_pc";

        //Read header file (.cterrain_pc)
        Stream cpuFile = PackfileVFS.OpenFile(cpuFilePath) ?? throw new Exception($"Importer failed to load .cterrain_pc file from {cpuFilePath}");
        Stream gpuFile = PackfileVFS.OpenFile(gpuFilePath) ?? throw new Exception($"Importer failed to load .gterrain_pc file from {gpuFilePath}");
        terrainFile = new(name);
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

        terrain.MaterialNames = terrainFile.TerrainMaterialNames.Select(nameAndOffset => nameAndOffset.Item1).ToList();
    }

    private void LoadSubzones(string packfileName, string name, Territory territory, ZoneTerrain terrain, int subzoneIndex, List<EditorObject> createdObjects,
        TerrainFile terrainFile)
    {
        string subzoneName = $"{name}_{subzoneIndex}";
        using Stream subzoneCpuFile = PackfileVFS.OpenFile($"//data/{packfileName}/ns_base.str2_pc/{subzoneName}.ctmesh_pc") ??
                                      throw new Exception($"Failed to open ctmesh {subzoneIndex} in {packfileName}");
        using Stream subzoneGpuFile = PackfileVFS.OpenFile($"//data/{packfileName}/ns_base.str2_pc/{subzoneName}.gtmesh_pc") ??
                                      throw new Exception($"Failed to open gtmesh {subzoneIndex} in {packfileName}");

        TerrainSubzoneFile subzoneFile = new(subzoneName);
        subzoneFile.ReadHeader(subzoneCpuFile);

        TerrainSubzone subzone = new TerrainSubzone { Name = subzoneName };
        subzone.Position = subzoneFile.Data.Position;
        createdObjects.Add(subzone);

        //Load subzone high lod terrain mesh
        {
            MeshInstanceData terrainMesh = subzoneFile.ReadTerrainMeshData(subzoneGpuFile);
            ProjectBuffer indexBuffer = NanoDB.CreateBuffer(terrainMesh.Indices, $"{name}_high_lod_{subzoneIndex}_indices") ??
                                        throw new Exception($"Failed to create buffer for indices of subzone mesh {subzoneIndex}for {name}");
            ProjectBuffer vertexBuffer = NanoDB.CreateBuffer(terrainMesh.Vertices, $"{name}_high_lod_{subzoneIndex}_vertices") ??
                                         throw new Exception($"Failed to create buffer for vertices of subzone mesh {subzoneIndex} for {name}");
            ProjectMesh mesh = new() { Name = $"{name}_high_lod_{subzoneIndex}_mesh" };
            mesh.InitFromRfgMeshConfig(terrainMesh.Config);
            mesh.IndexBuffer = indexBuffer;
            mesh.VertexBuffer = vertexBuffer;
            subzone.Mesh = mesh;
            createdObjects.Add(mesh);
        }

        //Load stitch mesh
        subzone.HasStitchMeshes = subzoneFile.HasStitchMesh;
        if (subzoneFile.HasStitchMesh)
        {
            MeshInstanceData stitchMesh = subzoneFile.ReadStitchMeshData(subzoneGpuFile);
            ProjectBuffer indexBuffer = NanoDB.CreateBuffer(stitchMesh.Indices, $"{name}_subzone{subzoneIndex}_stitch_indices") ??
                                        throw new Exception($"Failed to create buffer for indices of subzone stitch mesh {subzoneIndex}for {name}");
            ProjectBuffer vertexBuffer = NanoDB.CreateBuffer(stitchMesh.Vertices, $"{name}_subzone{subzoneIndex}_stitch_vertices") ??
                                         throw new Exception($"Failed to create buffer for vertices of subzone stitch mesh {subzoneIndex} for {name}");
            ProjectMesh mesh = new() { Name = $"{name}_subzone{subzoneIndex}_stitch_mesh" };
            mesh.InitFromRfgMeshConfig(stitchMesh.Config);
            mesh.IndexBuffer = indexBuffer;
            mesh.VertexBuffer = vertexBuffer;
            subzone.StitchMesh = mesh;
            createdObjects.Add(mesh);
        }

        //TODO: Load road meshes here. Not implemented for now since MP/WC maps don't have them

        //Load rock meshes
        for (int stitchIndex = 0; stitchIndex < subzoneFile.StitchInstances.Count; stitchIndex++)
        {
            TerrainStitchInstance stitchInstance = subzoneFile.StitchInstances[stitchIndex];
            string stitchPieceName = subzoneFile.StitchPieceNames2[stitchIndex];
            LoadRock(packfileName, stitchInstance, stitchPieceName, territory, createdObjects);
        }
        
        terrain.Subzones[subzoneIndex] = subzone;
    }

    private void LoadRock(string packfileName, TerrainStitchInstance stitchInstance, string stitchPieceName, Territory territory, List<EditorObject> createdObjects)
    {
        try
        {
            if (stitchPieceName.Contains("_Road", StringComparison.OrdinalIgnoreCase))
                return; //Skip road stitch instance. Road mesh data is stored in the terrain file and loaded separately

            Rock? rockSearch = territory.Rocks.FirstOrDefault(rock => rock?.Name == stitchPieceName, null);
            if (rockSearch != null && rockSearch.Mesh != null)
            {
                Rock cachedRock = rockSearch;
                Rock cachedRockCopy = new Rock(stitchPieceName, stitchInstance.Position, stitchInstance.Rotation.ToMatrix4x4(), cachedRock.Mesh, cachedRock.DiffuseTexture);
                createdObjects.Add(cachedRockCopy);
                territory.Rocks.Add(cachedRockCopy);
                return;
            }

            //First time loading the rock. Must load the rfg files and extract the meshes + textures
            using Stream? rockCpuFile = PackfileVFS.OpenFile($"//data/{packfileName}/ns_base.str2_pc/{stitchPieceName}.cstch_pc");
            using Stream? rockGpuFile = PackfileVFS.OpenFile($"//data/{packfileName}/ns_base.str2_pc/{stitchPieceName}.gstch_pc");
            if (rockCpuFile is null || rockGpuFile is null)
            {
                Log.Error($"Failed to open rock files for {stitchPieceName} in {packfileName}. Skipping rock.");
                return;
            }

            RockMeshFile rockFile = new RockMeshFile(stitchPieceName);
            rockFile.ReadHeader(rockCpuFile);

            MeshInstanceData meshData = rockFile.ReadData(rockGpuFile);
            ProjectBuffer indexBuffer = NanoDB.CreateBuffer(meshData.Indices, $"{Path.GetFileNameWithoutExtension(packfileName)}_{stitchPieceName}_indices") ??
                                        throw new Exception($"Failed to create buffer for indices of rock {stitchPieceName}");
            ProjectBuffer vertexBuffer = NanoDB.CreateBuffer(meshData.Vertices, $"{Path.GetFileNameWithoutExtension(packfileName)}_{stitchPieceName}_vertices") ??
                                         throw new Exception($"Failed to create buffer for vertices of rock {stitchPieceName}");
            ProjectMesh mesh = new() { Name = $"{stitchPieceName}.cstch_pc" };
            mesh.InitFromRfgMeshConfig(meshData.Config);
            mesh.IndexBuffer = indexBuffer;
            mesh.VertexBuffer = vertexBuffer;
            createdObjects.Add(mesh);

            //Load rock textures
            ProjectTexture? diffuseTexture = null;
            if (rockFile.TextureNames.Count >= 1)
            {
                diffuseTexture = GetOrLoadTerrainTexture(rockFile.TextureNames[0], createdObjects);
            }
            if (diffuseTexture == null)
            {
                Log.Warning($"Failed to load diffuse texture for {stitchPieceName}");
            }

            Rock rock = new Rock(stitchPieceName, stitchInstance.Position, stitchInstance.Rotation.ToMatrix4x4(), mesh, diffuseTexture);
            createdObjects.Add(rock);
            territory.Rocks.Add(rock);
        }
        catch (Exception ex)
        {
            Log.Error(ex, $"Error loading rock for {stitchPieceName} in {packfileName}.");
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