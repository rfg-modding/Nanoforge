using System;
using System.Net.Mime;
using Silk.NET.Vulkan;
using Buffer = Silk.NET.Vulkan.Buffer;

namespace Nanoforge.Render.Resources;

public unsafe class Texture2D : VkMemory
{
    private readonly uint _width;
    private readonly uint _height;
    private readonly uint _mipLevels;
    private readonly Format _format;
    private readonly ImageTiling _tiling;
    private readonly ImageUsageFlags _usage;
    private readonly MemoryPropertyFlags _properties;
    private readonly ImageAspectFlags _imageAspectFlags;

    private Image _textureImage;
    private ImageView _textureImageView;
    private Sampler _textureSampler;
    private ImageLayout _currentLayout;
    
    public Extent3D ImageSize => new(_width, _height, 1);
    
    public Image Handle => _textureImage;
    public ImageView ImageViewHandle => _textureImageView;
    public Sampler SamplerHandle => _textureSampler;

    public bool Destroyed { get; private set; } = false;
    
    public Texture2D(RenderContext context, uint width, uint height, uint mipLevels, Format format, ImageTiling tiling, ImageUsageFlags usage, MemoryPropertyFlags properties, ImageAspectFlags imageAspectFlags): base(context)
    {
        _width = width;
        _height = height;
        _mipLevels = mipLevels;
        _format = format;
        _tiling = tiling;
        _usage = usage;
        _properties = properties;
        _imageAspectFlags = imageAspectFlags;
        _currentLayout = ImageLayout.Undefined;

        CreateImage();
    }
    
    private void CreateImage()
    {
        ImageCreateInfo imageInfo = new()
        {
            SType = StructureType.ImageCreateInfo,
            ImageType = ImageType.Type2D,
            Extent =
            {
                Width = _width,
                Height = _height,
                Depth = 1,
            },
            MipLevels = _mipLevels,
            ArrayLayers = 1,
            Format = _format,
            Tiling = _tiling,
            InitialLayout = ImageLayout.Undefined,
            Usage = _usage,
            Samples = SampleCountFlags.Count1Bit,
            SharingMode = SharingMode.Exclusive,
        };

        fixed (Image* imagePtr = &_textureImage)
        {
            if (Context.Vk.CreateImage(Context.Device, in imageInfo, null, imagePtr) != Result.Success)
            {
                throw new Exception("Failed to create Texture2D image!");
            }
        }

        Context.Vk.GetImageMemoryRequirements(Context.Device, _textureImage, out MemoryRequirements memRequirements);

        MemoryAllocateInfo allocInfo = new()
        {
            SType = StructureType.MemoryAllocateInfo,
            AllocationSize = memRequirements.Size,
            MemoryTypeIndex = Context.FindMemoryType(memRequirements.MemoryTypeBits, _properties),
        };

        fixed (DeviceMemory* imageMemoryPtr = &Memory)
        {
            if (Context.Vk.AllocateMemory(Context.Device, in allocInfo, null, imageMemoryPtr) != Result.Success)
            {
                throw new Exception("Failed to allocate Texture2D image memory!");
            }
        }

        Context.Vk.BindImageMemory(Context.Device, _textureImage, Memory, 0);
    }
    
    public void SetPixels(byte[] pixels, CommandPool pool, Queue queue, bool generateMipMaps = false)
    {
        TransitionLayoutImmediate(ImageLayout.TransferDstOptimal, pool, queue);
        Context.StagingBuffer.SetData(pixels);
        Context.StagingBuffer.CopyToImage(_textureImage, _width, _height, pool, queue); 
        TransitionLayoutImmediate(ImageLayout.ShaderReadOnlyOptimal, pool, queue);

        if (generateMipMaps)
        {
            GenerateMipMaps(Context, _textureImage, _format, _width, _height, _mipLevels, pool, queue);
        }
    }

    public void TransitionLayoutImmediate(ImageLayout newLayout, CommandPool pool, Queue queue)
    {
        CommandBuffer cmd = Context.BeginSingleTimeCommands(pool);
        TransitionLayout(cmd, newLayout);
        Context.EndSingleTimeCommands(cmd, pool, queue);
    }
    
    public void TransitionLayout(CommandBuffer cmd, ImageLayout newLayout)
    {
        ImageMemoryBarrier barrier = new()
        {
            SType = StructureType.ImageMemoryBarrier,
            OldLayout = _currentLayout,
            NewLayout = newLayout,
            SrcQueueFamilyIndex = Vk.QueueFamilyIgnored,
            DstQueueFamilyIndex = Vk.QueueFamilyIgnored,
            Image = _textureImage,
            SubresourceRange =
            {
                AspectMask = ImageAspectFlags.ColorBit,
                BaseMipLevel = 0,
                LevelCount = _mipLevels,
                BaseArrayLayer = 0,
                LayerCount = 1,
            }
        };

        PipelineStageFlags sourceStage;
        PipelineStageFlags destinationStage;

        switch (_currentLayout)
        {
            case ImageLayout.Undefined:
                barrier.SrcAccessMask = 0;
                sourceStage = PipelineStageFlags.TopOfPipeBit;
                break;
            case ImageLayout.TransferSrcOptimal:
                barrier.SrcAccessMask = AccessFlags.TransferReadBit;
                sourceStage = PipelineStageFlags.TransferBit;
                break;
            case ImageLayout.TransferDstOptimal:
                barrier.SrcAccessMask = AccessFlags.TransferWriteBit;
                sourceStage = PipelineStageFlags.TransferBit;
                break;
            case ImageLayout.General:
                barrier.SrcAccessMask = AccessFlags.ShaderReadBit;
                sourceStage = PipelineStageFlags.ComputeShaderBit;
                break;
            default:
                throw new Exception($"Unsupported image layout transition from {_currentLayout} to {newLayout}");
        }

        switch (newLayout)
        {
            case ImageLayout.TransferSrcOptimal:
                barrier.DstAccessMask = AccessFlags.TransferReadBit;
                destinationStage = PipelineStageFlags.TransferBit;
                break;
            case ImageLayout.TransferDstOptimal:
                barrier.DstAccessMask = AccessFlags.TransferWriteBit;
                destinationStage = PipelineStageFlags.TransferBit;
                break;
            case ImageLayout.General:
                barrier.DstAccessMask = AccessFlags.ShaderReadBit;
                destinationStage = PipelineStageFlags.ComputeShaderBit;
                break;
            case ImageLayout.ShaderReadOnlyOptimal:
                barrier.DstAccessMask = AccessFlags.ShaderReadBit;
                destinationStage = PipelineStageFlags.FragmentShaderBit;
                break;
            default:
                throw new Exception($"Unsupported image layout transition from {_currentLayout} to {newLayout}");
        }

        Context.Vk.CmdPipelineBarrier(cmd, sourceStage, destinationStage, 0, 0, null, 0, null, 1, in barrier);
        _currentLayout = newLayout;
    }
    
    private static void GenerateMipMaps(RenderContext context, Image image, Format imageFormat, uint width, uint height, uint mipLevels, CommandPool pool, Queue queue)
    {
        context.Vk.GetPhysicalDeviceFormatProperties(context.PhysicalDevice, imageFormat, out var formatProperties);

        if ((formatProperties.OptimalTilingFeatures & FormatFeatureFlags.SampledImageFilterLinearBit) == 0)
        {
            throw new Exception("texture image format does not support linear blitting!");
        }

        var commandBuffer = context.BeginSingleTimeCommands(pool);

        ImageMemoryBarrier barrier = new()
        {
            SType = StructureType.ImageMemoryBarrier,
            Image = image,
            SrcQueueFamilyIndex = Vk.QueueFamilyIgnored,
            DstQueueFamilyIndex = Vk.QueueFamilyIgnored,
            SubresourceRange =
            {
                AspectMask = ImageAspectFlags.ColorBit,
                BaseArrayLayer = 0,
                LayerCount = 1,
                LevelCount = 1,
            }
        };

        var mipWidth = width;
        var mipHeight = height;

        for (uint i = 1; i < mipLevels; i++)
        {
            barrier.SubresourceRange.BaseMipLevel = i - 1;
            barrier.OldLayout = ImageLayout.TransferDstOptimal;
            barrier.NewLayout = ImageLayout.TransferSrcOptimal;
            barrier.SrcAccessMask = AccessFlags.TransferWriteBit;
            barrier.DstAccessMask = AccessFlags.TransferReadBit;

            context.Vk.CmdPipelineBarrier(commandBuffer, PipelineStageFlags.TransferBit, PipelineStageFlags.TransferBit, 0,
                0, null,
                0, null,
                1, in barrier);

            ImageBlit blit = new()
            {
                SrcOffsets =
                {
                    Element0 = new Offset3D(0,0,0),
                    Element1 = new Offset3D((int)mipWidth, (int)mipHeight, 1),
                },
                SrcSubresource =
                {
                    AspectMask = ImageAspectFlags.ColorBit,
                    MipLevel = i - 1,
                    BaseArrayLayer = 0,
                    LayerCount = 1,
                },
                DstOffsets =
                {
                    Element0 = new Offset3D(0,0,0),
                    Element1 = new Offset3D((int)(mipWidth > 1 ? mipWidth / 2 : 1), (int)(mipHeight > 1 ? mipHeight / 2 : 1),1),
                },
                DstSubresource =
                {
                    AspectMask = ImageAspectFlags.ColorBit,
                    MipLevel = i,
                    BaseArrayLayer = 0,
                    LayerCount = 1,
                },

            };

            context.Vk.CmdBlitImage(commandBuffer,
                image, ImageLayout.TransferSrcOptimal,
                image, ImageLayout.TransferDstOptimal,
                1, in blit,
                Filter.Linear);

            barrier.OldLayout = ImageLayout.TransferSrcOptimal;
            barrier.NewLayout = ImageLayout.ShaderReadOnlyOptimal;
            barrier.SrcAccessMask = AccessFlags.TransferReadBit;
            barrier.DstAccessMask = AccessFlags.ShaderReadBit;

            context.Vk.CmdPipelineBarrier(commandBuffer, PipelineStageFlags.TransferBit, PipelineStageFlags.FragmentShaderBit, 0,
                0, null,
                0, null,
                1, in barrier);

            if (mipWidth > 1) mipWidth /= 2;
            if (mipHeight > 1) mipHeight /= 2;
        }

        barrier.SubresourceRange.BaseMipLevel = mipLevels - 1;
        barrier.OldLayout = ImageLayout.TransferDstOptimal;
        barrier.NewLayout = ImageLayout.ShaderReadOnlyOptimal;
        barrier.SrcAccessMask = AccessFlags.TransferWriteBit;
        barrier.DstAccessMask = AccessFlags.ShaderReadBit;

        context.Vk.CmdPipelineBarrier(commandBuffer, PipelineStageFlags.TransferBit, PipelineStageFlags.FragmentShaderBit, 0,
            0, null,
            0, null,
            1, in barrier);

        context.EndSingleTimeCommands(commandBuffer, pool, queue);
    }

    public void CreateImageView()
    {
        ImageViewCreateInfo createInfo = new()
        {
            SType = StructureType.ImageViewCreateInfo,
            Image = _textureImage,
            ViewType = ImageViewType.Type2D,
            Format = _format,
            SubresourceRange =
            {
                AspectMask = _imageAspectFlags,
                BaseMipLevel = 0,
                LevelCount = _mipLevels,
                BaseArrayLayer = 0,
                LayerCount = 1,
            }

        };

        if (Vk.CreateImageView(Device, in createInfo, null, out _textureImageView) != Result.Success)
        {
            throw new Exception("Failed to create Texture2D imageview");
        }
    }
    
    public void CreateTextureSampler()
    {
        Vk.GetPhysicalDeviceProperties(Context.PhysicalDevice, out PhysicalDeviceProperties properties);
    
        SamplerCreateInfo samplerInfo = new()
        {
            SType = StructureType.SamplerCreateInfo,
            MagFilter = Filter.Linear,
            MinFilter = Filter.Linear,
            AddressModeU = SamplerAddressMode.Repeat,
            AddressModeV = SamplerAddressMode.Repeat,
            AddressModeW = SamplerAddressMode.Repeat,
            AnisotropyEnable = true,
            MaxAnisotropy = properties.Limits.MaxSamplerAnisotropy,
            BorderColor = BorderColor.IntOpaqueBlack,
            UnnormalizedCoordinates = false,
            CompareEnable = false,
            CompareOp = CompareOp.Always,
            MipmapMode = SamplerMipmapMode.Linear,
            MinLod = 0,
            MaxLod = _mipLevels,
            MipLodBias = 0,
        };

        fixed (Sampler* samplerPtr = &_textureSampler)
        {
            if (Vk.CreateSampler(Device, in samplerInfo, null, samplerPtr) != Result.Success)
            {
                throw new Exception("Failed to create Texture2D sampler");
            }   
        }
    }
    
    public void Destroy()
    {
        if (Destroyed)
            return;
        
        Vk.DestroySampler(Device, _textureSampler, null);
        Vk.DestroyImageView(Device, _textureImageView, null);
        Vk.DestroyImage(Device, _textureImage, null);
        Vk.FreeMemory(Device, Memory, null);
        Destroyed = true;
    }

    public void CopyToBuffer(CommandBuffer cmd, Buffer buffer)
    {
        var layers = new ImageSubresourceLayers(ImageAspectFlags.ColorBit, 0, 0, 1);
        var copyRegion = new BufferImageCopy(0, 0, 0, layers, default, ImageSize);
        Vk.CmdCopyImageToBuffer(cmd, _textureImage, _currentLayout, buffer, 1, copyRegion);
    }

    public static Texture2D FromFile(RenderContext context, CommandPool pool, Queue queue, string path)
    {
        using var img = SixLabors.ImageSharp.Image.Load<SixLabors.ImageSharp.PixelFormats.Rgba32>(path);
        ulong imageSize = (ulong)(img.Width * img.Height * img.PixelType.BitsPerPixel / 8);
        uint mipLevels = (uint)(Math.Floor(Math.Log2(Math.Max(img.Width, img.Height))) + 1);

        byte[] pixelData = new byte[imageSize];
        img.CopyPixelDataTo(pixelData);

        //TODO: Support more texture formats. Actually check what format ImageSharp returns instead of hardcoding it.
        Format vulkanFormat = Format.R8G8B8A8Srgb;
        
        Texture2D texture = new Texture2D(context, (uint)img.Width, (uint)img.Height, mipLevels, vulkanFormat,
            ImageTiling.Optimal, ImageUsageFlags.TransferSrcBit | ImageUsageFlags.TransferDstBit | ImageUsageFlags.SampledBit, MemoryPropertyFlags.DeviceLocalBit,
            ImageAspectFlags.ColorBit);
        texture.SetPixels(pixelData, pool, queue, generateMipMaps: false);
        texture.CreateTextureSampler();
        texture.CreateImageView();

        return texture;
    }
}