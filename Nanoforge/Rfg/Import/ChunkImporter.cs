using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using Nanoforge.Editor;
using Nanoforge.FileSystem;
using Nanoforge.Gui.ViewModels.Documents.ChunkViewer;
using RFGM.Formats.Materials;
using RFGM.Formats.Meshes;
using RFGM.Formats.Meshes.Chunks;
using RFGM.Formats.Meshes.Shared;
using RFGM.Formats.Peg;
using RFGM.Formats.Peg.Models;
using Serilog;
using Silk.NET.Vulkan;

namespace Nanoforge.Rfg.Import;

public class ChunkImporter
{
    public Chunk? ImportChunk(string packfileName, Territory map, Zone zone, string chunkName, List<EditorObject> createdObjects)
    {
        try
        {
            //TODO: Need to index different files for SP/SP DLC
            //TODO: Indexing the entire packfile at runtime probably isn't going to be feasible for SP
            //Index textures in this maps packfile + common file so textures can be found
            if (!TextureIndex.IsIndexed(packfileName))
            {
                TextureIndex.IndexVpp(packfileName);
            }
            if (!TextureIndex.IsIndexed("mp_common.vpp_pc"))
            {
                TextureIndex.IndexVpp("mp_common.vpp_pc");
            }

            using Stream cpuFile = PackfileVFS.OpenFile($"//data/{packfileName}/ns_base.str2_pc/{chunkName}.cchk_pc") ?? throw new Exception($"Failed to open cchk_pc file");
            using Stream gpuFile = PackfileVFS.OpenFile($"//data/{packfileName}/ns_base.str2_pc/{chunkName}.gchk_pc") ?? throw new Exception($"Failed to open gchk_pc file");

            ChunkFile chunkFile = new(chunkName);
            chunkFile.ReadHeader(cpuFile);

            Chunk chunk = new() { Name = chunkName };
            foreach (Destroyable destroyable in chunkFile.Destroyables)
            {
                chunk.Destroyables.Add(destroyable);
            }
            chunk.Identifiers = chunkFile.Identifiers;

            MeshInstanceData chunkMesh = chunkFile.ReadData(gpuFile);
            ProjectBuffer indexBuffer = NanoDB.CreateBuffer(chunkMesh.Indices, $"{chunkName}_indices") ??
                                        throw new Exception($"Failed to create buffer for indices of {chunkName}");
            ProjectBuffer vertexBuffer = NanoDB.CreateBuffer(chunkMesh.Vertices, $"{chunkName}_vertices") ??
                                         throw new Exception($"Failed to create buffer for vertices of {chunkName}");
            ProjectMesh mesh = new() { Name = $"{chunkName}_mesh" };
            mesh.InitFromRfgMeshConfig(chunkMesh.Config);
            mesh.IndexBuffer = indexBuffer;
            mesh.VertexBuffer = vertexBuffer;
            chunk.Mesh = mesh;
            createdObjects.Add(mesh);

            //Setup materials and textures
            foreach (RfgMaterial material in chunkFile.Materials)
            {
                TextureType[] textureTypesToLoad = [TextureType.Diffuse, TextureType.Normal, TextureType.Specular];
                List<ProjectTexture?> textures = new();

                foreach (TextureType textureType in textureTypesToLoad)
                {
                    string? textureName = material.Textures.Select(texture => texture.Name).FirstOrDefault(textureName => ClassifyTexture(textureName!) == textureType, null);
                    if (textureName == null)
                    {
                        textures.Add(null); //The loader code will use the missing texture
                        //Log.Warning($"Failed to find texture with type {textureType} on {chunkName} material {material.Name}");
                    }
                    else
                    {
                        ProjectTexture? texture = GetOrLoadChunkTexture(textureName, createdObjects);
                        if (texture == null)
                        {
                            Log.Warning($"Failed to import texture {textureName} for {chunkName} material {material.Name}");
                            textures.Add(null);
                        }
                        else
                        {
                            textures.Add(texture);
                        }
                    }
                }

                chunk.Materials.Add(material);
                chunk.Textures.Add(textures);
            }

            createdObjects.Add(chunk);
            return chunk;
        }
        catch (Exception ex)
        {
            Log.Error(ex, $"Error while importing chunk {chunkName} in {packfileName}");
            return null;
        }
    }

    //TODO: Stick this a importer helper class/file & remove GetOrLoadTerrainTexture from TerrainImporter.bf. They're identical
    //Get an RFG texture. If the tga hasn't been imported already it'll attempt to locate the peg containing the texture and import it.
    //All other textures in the peg will also be imported
    private ProjectTexture? GetOrLoadChunkTexture(string tgaName, List<EditorObject> createdObjects)
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

    private static TextureType ClassifyTexture(string textureName)
    {
        //There's a few that don't follow the naming scheme
        List<string> diffuseTextureNameExceptions =
        [
            "edf_mc_compmetal_01.tga", "edf_mc_compmetal_01.tga", "LargeSign_Mason_Wanted.tga", "locker_poster_WA.tga", "WA_Animated_Eos_Bits.tga", "LargeSign_Comp_FG.tga",
            "LargeSign_Mason.tga", "ore_mineral_decal.tga"
        ];

        if (textureName.EndsWith("_d.tga", StringComparison.OrdinalIgnoreCase) ||
            diffuseTextureNameExceptions.Any(name => string.Equals(name, textureName, StringComparison.OrdinalIgnoreCase)))
        {
            return TextureType.Diffuse;
        }

        if (textureName.EndsWith("_s.tga", StringComparison.OrdinalIgnoreCase))
        {
            return TextureType.Specular;
        }

        if (textureName.EndsWith("_n.tga", StringComparison.OrdinalIgnoreCase) || string.Equals(textureName, "flat-normalmap.tga", StringComparison.OrdinalIgnoreCase))
        {
            return TextureType.Normal;
        }

        if (string.Equals(textureName, "missing.tga", StringComparison.OrdinalIgnoreCase))
        {
            return TextureType.Missing;
        }

        if (textureName.StartsWith("dcl_", StringComparison.OrdinalIgnoreCase) || textureName.Contains("_decal_", StringComparison.OrdinalIgnoreCase) ||
            textureName.EndsWith("_x.tga", StringComparison.OrdinalIgnoreCase))
        {
            return TextureType.Decal;
        }

        return TextureType.Unknown;
    }
}