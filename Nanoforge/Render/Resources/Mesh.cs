using System;
using Silk.NET.Vulkan;
using Buffer = Silk.NET.Vulkan.Buffer;

namespace Nanoforge.Render.Resources;

public class Mesh
{
    private readonly RenderContext _context;
    private readonly VkBuffer _vertexBuffer;
    private readonly VkBuffer _indexBuffer;
    public readonly uint VertexCount;
    public readonly uint IndexCount;
    private readonly IndexType _indexType;
    
    public Buffer VertexBufferHandle => _vertexBuffer.VkHandle;
    public Buffer IndexBufferHandle => _indexBuffer.VkHandle;

    public Mesh(RenderContext context, Span<byte> vertices, Span<byte> indices, uint vertexCount, uint indexCount, uint indexSize)
    {
        _context = context;
        VertexCount = vertexCount;
        IndexCount = indexCount;
        
        _vertexBuffer = new VkBuffer(_context, (ulong)vertices.Length, BufferUsageFlags.TransferDstBit | BufferUsageFlags.VertexBufferBit, MemoryPropertyFlags.DeviceLocalBit);
        _context.StagingBuffer.SetData(vertices);
        _context.StagingBuffer.CopyTo(_vertexBuffer, (ulong)vertices.Length);
        
        _indexBuffer = new VkBuffer(_context, (ulong)indices.Length, BufferUsageFlags.TransferDstBit | BufferUsageFlags.IndexBufferBit, MemoryPropertyFlags.DeviceLocalBit);
        _context.StagingBuffer.SetData(indices);
        _context.StagingBuffer.CopyTo(_indexBuffer, (ulong)indices.Length);

        _indexType = indexSize switch
        {
            2 => IndexType.Uint16,
            4 => IndexType.Uint32,
            _ => throw new Exception($"Mesh created with unsupported index size of {indexSize} bytes")
        };
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
        _vertexBuffer.Destroy();
        _indexBuffer.Destroy();
    }
}