using System;
using System.Collections.Generic;
using System.Numerics;
using System.Runtime.CompilerServices;
using Nanoforge.Render.Materials;
using Nanoforge.Render.Misc;
using RFGM.Formats.Meshes.Shared;
using Serilog;
using Silk.NET.Vulkan;

namespace Nanoforge.Render.Resources;

public class SimpleRenderObject : RenderObjectBase
{
    public Mesh Mesh;
    public Texture2D[] Textures = new Texture2D[10];
    public MaterialInstance Material;

    public SimpleRenderObject(Vector3 position, Matrix4x4 orient, Mesh mesh, Texture2D[] textures, string materialName) : base(position, orient, Vector3.One)
    {
        if (textures.Length > 10)
        {
            string err = $"Attempted to create a RenderObject with {textures.Length} textures. They can have 10 textures at most.";
            Log.Error(err);
            throw new Exception(err);
        }
        
        Position = position;
        Orient = orient;
        Mesh = mesh;
        for (var i = 0; i < textures.Length; i++)
        {
            Textures[i] = textures[i];
        }
        //Use default white texture when one isn't provided. It simplifies the code to just give every RenderObject 10 textures
        if (textures.Length < 10)
        {
            for (int i = textures.Length; i < 10; i++)
            {
                Textures[i] = Texture2D.MissingTexture;
            }
        }
        Material = MaterialHelper.CreateMaterialInstance(materialName, Textures);
    }
    
    public override unsafe void WriteDrawCommands(List<RenderCommand> commands, Camera camera, ObjectConstantsWriter constants)
    {
        Matrix4x4 translation = Matrix4x4.CreateTranslation(Position);
        Matrix4x4 rotation = Orient;
        Matrix4x4 scale = Matrix4x4.CreateScale(Scale);
        Matrix4x4 model = rotation * translation * scale;
        PerObjectConstants objectConstants = new()
        {
            Model = model,
            WorldPosition = new Vector4(Position.X, Position.Y, Position.Z, 1.0f),
        };

        uint objectIndex = constants.NumObjects;
        constants.AddConstant(objectConstants);

        foreach (SubmeshData submesh in Mesh.Config.Submeshes)
        {
            uint firstBlock = submesh.RenderBlocksOffset;
            for (int j = 0; j < submesh.NumRenderBlocks; j++)
            {
                RenderBlock block = Mesh.Config.RenderBlocks[(int)(firstBlock + j)];
                commands.Add(new RenderCommand()
                {
                    MaterialInstance = Material,
                    Mesh = Mesh,
                    IndexCount = block.NumIndices,
                    StartIndex = block.StartIndex,
                    ObjectIndex = objectIndex,
                });
            }
        }
    }

    public override void Destroy()
    {
        Mesh.Destroy();
        foreach (Texture2D texture in Textures)
        {
            TextureManager.RemoveReference(texture);
        }
    }
}