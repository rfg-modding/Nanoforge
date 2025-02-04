using System;
using System.Numerics;
using System.Runtime.CompilerServices;
using Nanoforge.Render.Materials;
using Nanoforge.Render.Misc;
using Serilog;
using Silk.NET.OpenAL;
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
    
    public virtual unsafe void Draw(RenderContext context, CommandBuffer commandBuffer, MaterialPipeline pipeline, uint swapchainImageIndex)
    {
        Matrix4x4 translation = Matrix4x4.CreateTranslation(Position);
        Matrix4x4 rotation = Orient;
        Matrix4x4 scale = Matrix4x4.CreateScale(Scale);
        Matrix4x4 model = rotation * translation * scale;
        PerObjectPushConstants pushConstants = new()
        {
            Model = model
        };
        context.Vk.CmdPushConstants(commandBuffer, pipeline.Layout, ShaderStageFlags.VertexBit, 0, (uint)Unsafe.SizeOf<PerObjectPushConstants>(), &model);
        
        Material.Bind(context, commandBuffer, swapchainImageIndex);
        Mesh.Draw(context, commandBuffer);
    }

    public virtual void Destroy()
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