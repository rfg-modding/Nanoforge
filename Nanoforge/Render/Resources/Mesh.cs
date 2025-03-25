using System;
using System.Collections.Generic;
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

    public readonly List<SubmeshData> Submeshes;
    public readonly List<RenderBlock> RenderBlocks;
    public readonly uint NumVertices;
    public readonly uint NumIndices;

    //TODO: Update this to take a ProjectMesh as input instead of MeshInstanceData
    //Span<byte> vertices, Span<byte> indices, uint vertexCount, uint indexCount, uint indexSize
    public Mesh(RenderContext context, RenderMeshData meshData, CommandPool pool, Queue queue)
    {
        _context = context;
        
        Submeshes = meshData.Submeshes;
        RenderBlocks = meshData.RenderBlocks;
        NumVertices = meshData.NumVertices;
        NumIndices = meshData.NumIndices;
        
        _vertexBuffer = new VkBuffer(_context, (ulong)meshData.Vertices.Length, BufferUsageFlags.TransferDstBit | BufferUsageFlags.VertexBufferBit, MemoryPropertyFlags.DeviceLocalBit);
        _context.StagingBuffer.SetData(meshData.Vertices);
        _context.StagingBuffer.CopyTo(_vertexBuffer, (ulong)meshData.Vertices.Length, pool, queue);
        
        _indexBuffer = new VkBuffer(_context, (ulong)meshData.Indices.Length, BufferUsageFlags.TransferDstBit | BufferUsageFlags.IndexBufferBit, MemoryPropertyFlags.DeviceLocalBit);
        _context.StagingBuffer.SetData(meshData.Indices);
        _context.StagingBuffer.CopyTo(_indexBuffer, (ulong)meshData.Indices.Length, pool, queue);

        _indexType = meshData.IndexType;
    }
    
    public unsafe void Bind(RenderContext context, CommandBuffer commandBuffer)
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
}