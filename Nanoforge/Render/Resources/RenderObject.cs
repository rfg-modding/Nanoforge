using System;
using System.Numerics;
using Nanoforge.Render.Materials;
using Serilog;
using Silk.NET.Vulkan;

namespace Nanoforge.Render.Resources;

public class RenderObject
{
    public Vector3 Position;
    public Matrix4x4 Orient;
    public Vector3 Scale = Vector3.One;

    public Mesh Mesh;
    public Texture2D[] Textures = new Texture2D[10];
    public MaterialInstance Material;

    public RenderObject(Vector3 position, Matrix4x4 orient, Mesh mesh, Texture2D[] textures, string materialName)
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
                Textures[i] = Texture2D.DefaultTexture;
            }
        }
        Material = MaterialHelper.CreateMaterialInstance(materialName, this);
    }
    
    public unsafe void Draw(RenderContext context, CommandBuffer commandBuffer, uint swapchainImageIndex)
    {
        Material.Bind(context, commandBuffer, swapchainImageIndex);
        Mesh.Bind(context, commandBuffer);
        context.Vk.CmdDrawIndexed(commandBuffer, Mesh.IndexCount, 1, 0, 0, 0);
    }

    public void Destroy()
    {
        Mesh.Destroy();
        foreach (Texture2D texture in Textures)
        {
            //TODO: Add reference counting system for renderer resources to avoid needing checks like this
            if (texture != Texture2D.DefaultTexture)
            {
                texture.Destroy();
            }
        }
    }
}