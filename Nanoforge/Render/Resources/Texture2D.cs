using System;
using System.Net.Mime;
using Serilog;
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

    public static Texture2D DefaultTexture { get; private set; } = null!;
    public static Texture2D MissingTexture { get; private set; } = null!;
    public static Texture2D FlatNormalMap { get; private set; } = null!;
    
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
    
    public void SetPixels(byte[] pixels, CommandPool pool, Queue queue)
    {
        TransitionLayoutImmediate(ImageLayout.TransferDstOptimal, pool, queue);
        Context.StagingBuffer.SetData(pixels);
        
        uint mipWidth = _width;
        uint mipHeight = _height;
        ulong offset = 0;
        for (uint i = 0; i < _mipLevels; i++)
        {
            BufferImageCopy region = new BufferImageCopy();
            region.BufferOffset = offset;
            region.BufferRowLength = 0;
            region.BufferImageHeight = 0;
            region.ImageSubresource = new ImageSubresourceLayers()
            {
                AspectMask = _imageAspectFlags,
                MipLevel = i,
                BaseArrayLayer = 0,
                LayerCount = 1,
            };
            region.ImageOffset = new Offset3D(0, 0, 0);
            region.ImageExtent = new Extent3D(Math.Max(mipWidth, 1), Math.Max(mipHeight, 1), 1);
            
            //TODO: DO COPY

            ulong mipSizeBytes = _format switch
            {
                Format.BC1RgbUnormBlock or Format.BC1RgbSrgbBlock => 8 * (mipWidth * mipHeight / (4 * 4)), //8 bytes per 4x4 pixel block,
                Format.BC2UnormBlock or Format.BC2SrgbBlock => 16 * (mipWidth * mipHeight / (4 * 4)), //16 bytes per 4x4 pixel block
                Format.BC3UnormBlock or Format.BC3SrgbBlock => 16 * (mipWidth * mipHeight / (4 * 4)), //16 bytes per 4x4 pixel block
                Format.R8G8B8A8Unorm or Format.R8G8B8A8Srgb => 4 * mipWidth * mipHeight, //RGBA, 1 byte per component
                _ => throw new ArgumentOutOfRangeException()
            };
            offset += mipSizeBytes;

            mipWidth = Math.Max(mipWidth / 2, 1);
            mipHeight = Math.Max(mipHeight / 2, 1);
            CommandBuffer commandBuffer = Context.BeginSingleTimeCommands(pool);
            Context.Vk.CmdCopyBufferToImage(commandBuffer, Context.StagingBuffer.VkHandle, _textureImage, ImageLayout.TransferDstOptimal, 1, in region);
            Context.EndSingleTimeCommands(commandBuffer, pool, queue);
        }
        
        TransitionLayoutImmediate(ImageLayout.ShaderReadOnlyOptimal, pool, queue);
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

    public static Texture2D? FromFile(RenderContext context, CommandPool pool, Queue queue, string path)
    {
        try
        {
            using var img = SixLabors.ImageSharp.Image.Load<SixLabors.ImageSharp.PixelFormats.Rgba32>(path);
            ulong imageSize = (ulong)(img.Width * img.Height * img.PixelType.BitsPerPixel / 8);
            uint mipLevels = 1;//(uint)(Math.Floor(Math.Log2(Math.Max(img.Width, img.Height))) + 1);

            byte[] pixelData = new byte[imageSize];
            img.CopyPixelDataTo(pixelData);

            //TODO: Support more texture formats. Actually check what format ImageSharp returns instead of hardcoding it.
            Format vulkanFormat = Format.R8G8B8A8Srgb;
        
            //TODO: Add option to generate mip maps before passing data into the constructor
            Texture2D texture = new Texture2D(context, (uint)img.Width, (uint)img.Height, mipLevels, vulkanFormat,
                ImageTiling.Optimal, ImageUsageFlags.TransferSrcBit | ImageUsageFlags.TransferDstBit | ImageUsageFlags.SampledBit, MemoryPropertyFlags.DeviceLocalBit,
                ImageAspectFlags.ColorBit);
            texture.SetPixels(pixelData, pool, queue);
            texture.CreateTextureSampler();
            texture.CreateImageView();

            return texture;
        }
        catch (Exception ex)
        {
            Log.Error(ex, $"Error creating Texture2D from file at {path}");
            return null;
        }
    }

    public static void LoadDefaultTextures(RenderContext context, CommandPool pool, Queue queue)
    {
        DefaultTexture = Texture2D.FromFile(context, pool, queue, $"{BuildConfig.AssetsDirectory}textures/White.png") ?? throw new Exception("Failed to load default Texture2D");
        MissingTexture = Texture2D.FromFile(context, pool, queue, $"{BuildConfig.AssetsDirectory}textures/Missing.png") ?? throw new Exception("Failed to load missing Texture2D");
        FlatNormalMap = Texture2D.FromFile(context, pool, queue, $"{BuildConfig.AssetsDirectory}textures/FlatNormalMap.png") ?? throw new Exception("Failed to load missing Texture2D");
    }
}