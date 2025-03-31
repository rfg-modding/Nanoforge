using System;
using System.Runtime.CompilerServices;
using Silk.NET.Vulkan;
using Buffer = Silk.NET.Vulkan.Buffer;

namespace Nanoforge.Render.Resources;

public unsafe class VkBuffer : VkMemory
{
    private Buffer _vkHandle;
    public Buffer VkHandle => _vkHandle;
    public ulong Size { get; private set; }

    private readonly BufferUsageFlags _usage;
    private readonly MemoryPropertyFlags _properties;

    public readonly bool CanGrow;
    
    public VkBuffer(RenderContext context, ulong size, BufferUsageFlags usage, MemoryPropertyFlags properties, bool canGrow = false) : base(context)
    {
        Size = size;
        _usage = usage;
        _properties = properties;
        CanGrow = canGrow;
        Init();
    }

    private void Init()
    {
        BufferCreateInfo bufferInfo = new()
        {
            SType = StructureType.BufferCreateInfo,
            Size = this.Size,
            Usage = this._usage,
            SharingMode = SharingMode.Exclusive,
        };

        fixed (Buffer* bufferPtr = &_vkHandle)
        {
            if (Vk.CreateBuffer(Device, in bufferInfo, null, bufferPtr) != Result.Success)
            {
                throw new Exception("failed to create vulkan buffer!");
            }
        }

        MemoryRequirements memRequirements = new();
        Vk.GetBufferMemoryRequirements(Device, VkHandle, out memRequirements);

        MemoryAllocateInfo allocateInfo = new()
        {
            SType = StructureType.MemoryAllocateInfo,
            AllocationSize = memRequirements.Size,
            MemoryTypeIndex = Context.FindMemoryType(memRequirements.MemoryTypeBits, _properties),
        };

        fixed (DeviceMemory* bufferMemoryPtr = &Memory)
        {
            if (Vk.AllocateMemory(Device, in allocateInfo, null, bufferMemoryPtr) != Result.Success)
            {
                throw new Exception("Failed to allocate VkBuffer memory!");
            }
        }

        Vk.BindBufferMemory(Device, VkHandle, Memory, 0);
    }

    public void SetData<T>(ref T data, ulong offset = 0) where T : unmanaged
    {
        fixed (void* ptr = &data)
        {
            int sizeInBytes = Unsafe.SizeOf<T>();
            SetData(new Span<byte>(ptr, sizeInBytes), offset);
        }
    }

    public void SetData<T>(Span<T> data, ulong offset = 0) where T : unmanaged
    {
        fixed (void* ptr = data)
        {
            int sizeInBytes = Unsafe.SizeOf<T>() * data.Length;
            SetData(new Span<byte>(ptr, sizeInBytes), offset);
        }
    }

    public void SetData(Span<byte> data, ulong offset = 0)
    {
        if ((ulong)data.Length > Size)
        {
            if (!CanGrow)
            {
                throw new Exception("Buffer size exceeded! Auto buffer resize not yet implemented!");
            }

            Console.WriteLine($"Growing buffer from {Size} bytes to {data.Length} bytes");
            Destroy();
            Size = (ulong)data.Length;
            Init();
        }
        
        void* ptr = null;
        MapMemory(ref ptr);
        byte* offsetPtr = (byte*)ptr;
        offsetPtr += offset;
        data.CopyTo(new Span<byte>(offsetPtr, data.Length));
        UnmapMemory();
    }

    public unsafe void Destroy()
    {
        Vk.DestroyBuffer(Device, VkHandle, null);
        Vk.FreeMemory(Device, Memory, null);
    }

    public void CopyTo(VkBuffer destination, ulong copySize, CommandPool pool, Queue queue)
    {
        CommandBuffer commandBuffer = Context.BeginSingleTimeCommands(pool);
        BufferCopy copyRegion = new()
        {
            Size = copySize,
        };

        Vk.CmdCopyBuffer(commandBuffer, srcBuffer: this.VkHandle, dstBuffer: destination.VkHandle, 1, in copyRegion);

        Context.EndSingleTimeCommands(commandBuffer, pool, queue);
    }
    
    public void CopyToImage(Image image, uint width, uint height, CommandPool pool, Queue queue)
    {
        CommandBuffer commandBuffer = Context.BeginSingleTimeCommands(pool);

        BufferImageCopy region = new()
        {
            BufferOffset = 0,
            BufferRowLength = 0,
            BufferImageHeight = 0,
            ImageSubresource =
            {
                AspectMask = ImageAspectFlags.ColorBit,
                MipLevel = 0,
                BaseArrayLayer = 0,
                LayerCount = 1,
            },
            ImageOffset = new Offset3D(0, 0, 0),
            ImageExtent = new Extent3D(width, height, 1),

        };

        Context.Vk.CmdCopyBufferToImage(commandBuffer, _vkHandle, image, ImageLayout.TransferDstOptimal, 1, in region);

        Context.EndSingleTimeCommands(commandBuffer, pool, queue);
    }
}