using System.Numerics;
using Nanoforge.Render.Materials;
using Silk.NET.Vulkan;

namespace Nanoforge.Render.Resources;

public class RenderObject
{
    public Vector3 Position;
    public Matrix4x4 Orient;
    public Vector3 Scale = Vector3.One;

    public Mesh Mesh;
    public Texture2D Texture;
    public MaterialInstance Material;

    public RenderObject(Vector3 position, Matrix4x4 orient, Mesh mesh, Texture2D texture, string materialName)
    {
        Position = position;
        Orient = orient;
        Mesh = mesh;
        Texture = texture;
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
        Texture.Destroy();
    }
}