using System.Numerics;
using System.Runtime.CompilerServices;
using Nanoforge.Render.Materials;
using Nanoforge.Render.Misc;
using RFGM.Formats.Meshes.Chunks;
using Silk.NET.Vulkan;

namespace Nanoforge.Render.Resources;

public class RenderChunk : RenderObject
{
    Destroyable _destroyable;
    
    public RenderChunk(Vector3 position, Matrix4x4 orient, Mesh mesh, Texture2D[] textures, string materialName, Destroyable destroyable) : base(position, orient, mesh, textures, materialName)
    {
        _destroyable = destroyable;
    }

    public override unsafe void Draw(RenderContext context, CommandBuffer commandBuffer, MaterialPipeline pipeline, uint swapchainImageIndex)
    {
        Material.Bind(context, commandBuffer, swapchainImageIndex);

        //Translation and rotation for the overall object
        Matrix4x4 objTranslation = Matrix4x4.CreateTranslation(Position);
        Matrix4x4 objRotation = Orient;
        
        //TODO: Get scaling working
        //Matrix4x4 objScale = Matrix4x4.CreateScale(Scale);
        
        //Render each chunk piece
        int dlodIndex = 0;
        foreach (Dlod dlod in _destroyable.Dlods)
        {
            Matrix4x4 chunkRotation = new Matrix4x4
            (
                dlod.Orient.M11, dlod.Orient.M12, dlod.Orient.M13, 0.0f,
                dlod.Orient.M21, dlod.Orient.M22, dlod.Orient.M23, 0.0f,
                dlod.Orient.M31, dlod.Orient.M32, dlod.Orient.M33, 0.0f,
                0.0f           , 0.0f           , 0.0f           , 1.0f
            );
            // Matrix4x4 chunkRotation = new Matrix4x4
            // (
            //     dlod.Orient.M11, dlod.Orient.M21, dlod.Orient.M31, 0.0f,
            //     dlod.Orient.M12, dlod.Orient.M22, dlod.Orient.M32, 0.0f,
            //     dlod.Orient.M13, dlod.Orient.M23, dlod.Orient.M33, 0.0f,
            //     0.0f           , 0.0f           , 0.0f           , 1.0f
            // );
            Matrix4x4 chunkTranslation = Matrix4x4.CreateTranslation(dlod.Pos);

            Matrix4x4 totalRotation = chunkRotation * objRotation;
            Matrix4x4 model = chunkRotation * chunkTranslation * objRotation * objTranslation;
            
            PerObjectPushConstants pushConstants = new()
            {
                Model = model
            };
            context.Vk.CmdPushConstants(commandBuffer, pipeline.Layout, ShaderStageFlags.VertexBit, 0, (uint)Unsafe.SizeOf<PerObjectPushConstants>(), &model);

            int numSubmeshOverrides = 0;
            for (int subpieceIndex = dlod.FirstPiece; subpieceIndex < dlod.FirstPiece + dlod.MaxPieces; subpieceIndex++)
            {
                ushort submeshIndex = _destroyable.SubpieceData[subpieceIndex].RenderSubpiece;
                Mesh.SubmeshOverrideIndices[numSubmeshOverrides++] = submeshIndex;
            }

            //if (dlodIndex == 0)
            {
                Mesh.Draw(context, commandBuffer, numSubmeshOverrides);
            }
            dlodIndex++;
        }
    }
    
    public override void Destroy()
    {
        Mesh.Destroy();
        foreach (Texture2D texture in Textures)
        {
            if (texture != Texture2D.DefaultTexture)
            {
                texture.Destroy();
            }
        }
    }
}