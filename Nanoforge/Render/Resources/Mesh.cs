using System;
using RFGM.Formats.Meshes.Shared;
using Silk.NET.Vulkan;
using Buffer = Silk.NET.Vulkan.Buffer;

namespace Nanoforge.Render.Resources;

public class Mesh
{
    private readonly RenderContext _context;
    private readonly VkBuffer _vertexBuffer;
    private readonly VkBuffer _indexBuffer;
    private readonly IndexType _indexType;
    
    public Buffer VertexBufferHandle => _vertexBuffer.VkHandle;
    public Buffer IndexBufferHandle => _indexBuffer.VkHandle;

    public bool Destroyed { get; private set; } = false;

    //TODO: Try to finally fix this BS
    //Bit of a hack used to render chunk meshes
    public static ushort[] SubmeshOverrideIndices = new ushort[256];

    public MeshConfig Config { get; }

    //TODO: Update this to take a ProjectMesh as input instead of MeshInstanceData
    //Span<byte> vertices, Span<byte> indices, uint vertexCount, uint indexCount, uint indexSize
    public Mesh(RenderContext context, MeshInstanceData meshInstance, CommandPool pool, Queue queue)
    {
        _context = context;
        Config = meshInstance.Config;
        
        _vertexBuffer = new VkBuffer(_context, (ulong)meshInstance.Vertices.Length, BufferUsageFlags.TransferDstBit | BufferUsageFlags.VertexBufferBit, MemoryPropertyFlags.DeviceLocalBit);
        _context.StagingBuffer.SetData(meshInstance.Vertices);
        _context.StagingBuffer.CopyTo(_vertexBuffer, (ulong)meshInstance.Vertices.Length, pool, queue);
        
        _indexBuffer = new VkBuffer(_context, (ulong)meshInstance.Indices.Length, BufferUsageFlags.TransferDstBit | BufferUsageFlags.IndexBufferBit, MemoryPropertyFlags.DeviceLocalBit);
        _context.StagingBuffer.SetData(meshInstance.Indices);
        _context.StagingBuffer.CopyTo(_indexBuffer, (ulong)meshInstance.Indices.Length, pool, queue);

        _indexType = Config.IndexSize switch
        {
            2 => IndexType.Uint16,
            4 => IndexType.Uint32,
            _ => throw new Exception($"Mesh created with unsupported index size of {Config.IndexSize} bytes")
        };
    }
    
    private unsafe void Bind(RenderContext context, CommandBuffer commandBuffer)
    {
        var vertexBuffers = new Buffer[] { VertexBufferHandle };
        var offsets = new ulong[] { 0 };

        fixed (ulong* offsetsPtr = offsets)
        fixed (Buffer* vertexBuffersPtr = vertexBuffers)
        {
            context.Vk.CmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffersPtr, offsetsPtr);
        }

        context.Vk.CmdBindIndexBuffer(commandBuffer, IndexBufferHandle, 0, _indexType);
    }

    public void Destroy()
    {
        //Easiest way to prevent objects shared by multiple RenderObjects from being destroyed multiple times
        //Ideally you'd have ref-counting so they'd only be destroyed when no objects reference them anymore, but currently NF doesn't require that.
        if (Destroyed)
            return;
        
        _vertexBuffer.Destroy();
        _indexBuffer.Destroy();
        Destroyed = true;
    }

    //TODO: Port logic to handle meshes with multiple LOD levels from older version of NF
    public void Draw(RenderContext context, CommandBuffer commandBuffer, int numSubmeshOverrides = -1)
    {
        Bind(context, commandBuffer);
        if (numSubmeshOverrides != -1)
        {
            //Special case used by RenderChunk to draw specific submeshes
            for (int i = 0; i < numSubmeshOverrides; i++)
            {
                ushort submeshIndex = SubmeshOverrideIndices[i];
                SubmeshData submesh = Config.Submeshes[submeshIndex];
                uint firstBlock = submesh.RenderBlocksOffset;
                for (int j = 0; j < submesh.NumRenderBlocks; j++)
                {
                    RenderBlock block = Config.RenderBlocks[(int)(firstBlock + j)];
                    context.Vk.CmdDrawIndexed(commandBuffer, block.NumIndices, 1, block.StartIndex, 0, 0);
                }
            }
        }
        else
        {
            context.Vk.CmdDrawIndexed(commandBuffer, Config.NumIndices, 1, 0, 0, 0);
        }
    }
}