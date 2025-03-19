using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Numerics;
using System.Runtime.CompilerServices;
using Nanoforge.Render.Materials;
using Nanoforge.Render.Misc;
using RFGM.Formats.Meshes.Chunks;
using RFGM.Formats.Meshes.Shared;
using Serilog;
using Silk.NET.Vulkan;

namespace Nanoforge.Render.Resources;

public class RenderChunk : RenderObjectBase
{
    public Mesh Mesh;
    public List<Texture2D[]> TexturesByMaterial = [];
    public List<MaterialPipeline> Materials = [];
    
    public Destroyable Destroyable { get; private set; }

    public RenderChunk(Vector3 position, Matrix4x4 orient, Mesh mesh, List<Texture2D[]> texturesByMaterial, string materialName, Destroyable destroyable) : base(position, orient, Vector3.One)
    {
        Position = position;
        Orient = orient;
        Mesh = mesh;
        Destroyable = destroyable;

        const int maxTexturesPerMaterial = 10;
        if (texturesByMaterial.Any(textures => textures.Length > maxTexturesPerMaterial))
        {
            string err = $"Attempted to create a RenderChunk with more than {maxTexturesPerMaterial} textures assigned to a material. They can have {maxTexturesPerMaterial} textures at most.";
            Log.Error(err);
            throw new Exception(err);
        }
        
        foreach (Texture2D[] textures in texturesByMaterial)
        {
            Texture2D[] textureArray = new Texture2D[maxTexturesPerMaterial];
            for (int i = 0; i < textures.Length; i++)
            {
                textureArray[i] = textures[i];
            }
            
            //Use default white texture when one isn't provided. It simplifies the code to just give every RenderObject 10 textures
            if (textures.Length < maxTexturesPerMaterial)
            {
                for (int i = textures.Length; i < maxTexturesPerMaterial; i++)
                {
                    textureArray[i] = Texture2D.MissingTexture;
                }
            }
            
            TexturesByMaterial.Add(textureArray);
        }
        
        foreach (Texture2D[] materialTextures in TexturesByMaterial)
        {
            MaterialPipeline pipeline = MaterialHelper.GetMaterialPipeline(materialName) ?? throw new NullReferenceException($"Failed to get material pipeline: '{materialName}'");
            Materials.Add(pipeline);
        }
        Debug.Assert(Materials.Count > 0);
        Debug.Assert(Materials.Count == TexturesByMaterial.Count);
    }

    public override void WriteDrawCommands(List<RenderCommand> commands, Camera camera, GpuFrameDataWriter constants)
    {
        //Translation and rotation for the overall object
        Matrix4x4 objTranslation = Matrix4x4.CreateTranslation(Position);
        Matrix4x4 objRotation = Orient;
        
        //TODO: Get scaling working
        //Matrix4x4 objScale = Matrix4x4.CreateScale(Scale);
        
        //Setup GPU material data
        List<(int gpuMaterialIndex, MaterialInstance material)> gpuMaterials = new();
        foreach (Texture2D[] textures in TexturesByMaterial)
        {
            MaterialInstance materialInstance = new()
            {
                Texture0 = textures[0].Index,
                Texture1 = textures[1].Index,
                Texture2 = textures[2].Index,
                Texture3 = textures[3].Index,
                Texture4 = textures[4].Index,
                Texture5 = textures[5].Index,
                Texture6 = textures[6].Index,
                Texture7 = textures[7].Index,
                Texture8 = textures[8].Index,
                Texture9 = textures[9].Index,
            };
            int gpuMaterialIndex = constants.AddMaterialInstance(materialInstance);
            gpuMaterials.Add((gpuMaterialIndex, materialInstance));
        }
        
        //Render each chunk piece
        foreach (Dlod dlod in Destroyable.Dlods)
        {
            Matrix4x4 chunkRotation = new Matrix4x4
            (
                dlod.Orient.M11, dlod.Orient.M12, dlod.Orient.M13, 0.0f,
                dlod.Orient.M21, dlod.Orient.M22, dlod.Orient.M23, 0.0f,
                dlod.Orient.M31, dlod.Orient.M32, dlod.Orient.M33, 0.0f,
                0.0f           , 0.0f           , 0.0f           , 1.0f
            );
            
            Matrix4x4 chunkTranslation = Matrix4x4.CreateTranslation(dlod.Pos);
            Matrix4x4 model = chunkRotation * chunkTranslation * objRotation * objTranslation;
            PerObjectConstants baseObjectConstants = new()
            {
                Model = model,
                WorldPosition = new Vector4(Position.X, Position.Y, Position.Z, 1.0f),
            };
            
            for (int subpieceIndex = dlod.FirstPiece; subpieceIndex < dlod.FirstPiece + dlod.MaxPieces; subpieceIndex++)
            {
                ushort submeshIndex = _destroyable.SubpieceData[subpieceIndex].RenderSubpiece;
                SubmeshData submesh = Mesh.Config.Submeshes[submeshIndex];
                uint firstBlock = submesh.RenderBlocksOffset;
                for (int j = 0; j < submesh.NumRenderBlocks; j++)
                {
                    RenderBlock block = Mesh.Config.RenderBlocks[(int)(firstBlock + j)];
                    MaterialPipeline pipeline = Materials[block.MaterialMapIndex];
                    
                    int materialIndex = gpuMaterials[block.MaterialMapIndex].gpuMaterialIndex;
                    PerObjectConstants perObjectConstants = baseObjectConstants with
                    {
                        MaterialIndex = materialIndex,
                    };
                    uint objectIndex = constants.AddObject(perObjectConstants);
                    
                    commands.Add(new RenderCommand
                    {
                        Pipeline = pipeline,
                        Mesh = Mesh,
                        IndexCount = block.NumIndices,
                        StartIndex = block.StartIndex,
                        ObjectIndex = objectIndex,
                    });
                }
            }
        }
    }
    
    public override void Destroy()
    {
        Mesh.Destroy();
        foreach (Texture2D[] textures in TexturesByMaterial)
        {
            foreach (Texture2D texture in textures)
            {
                TextureManager.RemoveReference(texture);
            }
        }
    }
}