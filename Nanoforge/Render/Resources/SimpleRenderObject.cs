using System;
using System.Collections.Generic;
using System.Numerics;
using Nanoforge.Render.Misc;
using RFGM.Formats.Meshes.Shared;
using Serilog;

namespace Nanoforge.Render.Resources;

public class SimpleRenderObject : RenderObjectBase
{
    public Mesh Mesh;
    public Texture2D[] Textures = new Texture2D[10];
    public readonly MaterialType MaterialType;

    public SimpleRenderObject(Vector3 position, Matrix4x4 orient, Mesh mesh, Texture2D[] textures, MaterialType materialType = MaterialType.Default) : base(position, orient, Vector3.One)
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

        MaterialType = materialType;
    }
    
    public override void WriteDrawCommands(List<RenderCommand> commands, Camera camera, GpuFrameDataWriter constants)
    {
        Matrix4x4 translation = Matrix4x4.CreateTranslation(Position);
        Matrix4x4 rotation = Orient;
        Matrix4x4 scale = Matrix4x4.CreateScale(Scale);
        Matrix4x4 model = rotation * translation * scale;

        MaterialInstance materialInstance = new()
        {
            Texture0 = Textures[0].Index,
            Texture1 = Textures[1].Index,
            Texture2 = Textures[2].Index,
            Texture3 = Textures[3].Index,
            Texture4 = Textures[4].Index,
            Texture5 = Textures[5].Index,
            Texture6 = Textures[6].Index,
            Texture7 = Textures[7].Index,
            Texture8 = Textures[8].Index,
            Texture9 = Textures[9].Index,
            Type = MaterialType,
        };
        int materialIndex = constants.AddMaterialInstance(materialInstance);
        
        PerObjectConstants objectConstants = new()
        {
            Model = model,
            WorldPosition = new Vector4(Position.X, Position.Y, Position.Z, 1.0f),
            MaterialIndex = materialIndex,
        };
        uint objectIndex = constants.AddObject(objectConstants);
        
        foreach (SubmeshData submesh in Mesh.Submeshes)
        {
            uint firstBlock = submesh.RenderBlocksOffset;
            for (int j = 0; j < submesh.NumRenderBlocks; j++)
            {
                RenderBlock block = Mesh.RenderBlocks[(int)(firstBlock + j)];
                commands.Add(new RenderCommand
                {
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