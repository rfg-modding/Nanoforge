
using System;
using System.Text.Json.Serialization;
using Nanoforge.Editor;
using Nanoforge.Render;
using Nanoforge.Render.Resources;
using RFGM.Formats.Peg.Models;
using Silk.NET.Vulkan;

namespace Nanoforge.Rfg;

public class ProjectTexture : EditorObject
{
    public ProjectBuffer? Data;
    //TODO: Change this to the equivalent for whichever graphics API ends up getting used in the renderer
    public Silk.NET.Vulkan.Format Format;
    public int Width;
    public int Height;
    public int NumMipLevels;

    [JsonConstructor]
    private ProjectTexture()
    {
        Data = null;
    }
    
    public ProjectTexture(ProjectBuffer data)
    {
        Data = data;
    }

    public ProjectTexture(byte[] data, string bufferName = "")
    {
        Data = NanoDB.CreateBuffer(data, bufferName) ?? throw new Exception($"Failed to create ProjectBuffer '{bufferName}' for ProjectTexture");
    }

    public static Silk.NET.Vulkan.Format PegFormatToVulkanFormat(RfgCpeg.Entry.BitmapFormat pegFormat, TextureFlags flags)
    {
        bool srgb = flags.HasFlag(TextureFlags.Srgb);
        Silk.NET.Vulkan.Format pixelFormat = (pegFormat, srgb) switch
        {
            (RfgCpeg.Entry.BitmapFormat.PcDxt1, false) => Silk.NET.Vulkan.Format.BC1RgbUnormBlock,
            (RfgCpeg.Entry.BitmapFormat.PcDxt1, true) => Silk.NET.Vulkan.Format.BC1RgbSrgbBlock,
            (RfgCpeg.Entry.BitmapFormat.PcDxt3, false) => Silk.NET.Vulkan.Format.BC2UnormBlock,
            (RfgCpeg.Entry.BitmapFormat.PcDxt3, true) => Silk.NET.Vulkan.Format.BC2SrgbBlock,
            (RfgCpeg.Entry.BitmapFormat.PcDxt5, false) => Silk.NET.Vulkan.Format.BC3UnormBlock,
            (RfgCpeg.Entry.BitmapFormat.PcDxt5, true) => Silk.NET.Vulkan.Format.BC3SrgbBlock,
            (RfgCpeg.Entry.BitmapFormat.Pc8888, false) => Silk.NET.Vulkan.Format.R8G8B8A8Unorm,
            (RfgCpeg.Entry.BitmapFormat.Pc8888, true) => Silk.NET.Vulkan.Format.R8G8B8A8Srgb,
            _ => throw new Exception($"Unsupported texture format {pegFormat} in TerrainImporter")
        };
        
        return pixelFormat;
    }

    public Texture2D? CreateRenderTexture(Renderer renderer, CommandPool pool, Queue queue)
    {
        if (Data == null)
            return null;

        byte[] pixels = Data.Load();
        Texture2D texture = new(renderer.Context, (uint)Width, (uint)Height, (uint)NumMipLevels, Format, ImageTiling.Optimal,
            ImageUsageFlags.TransferSrcBit | ImageUsageFlags.TransferDstBit | ImageUsageFlags.SampledBit,
            MemoryPropertyFlags.DeviceLocalBit,
            ImageAspectFlags.ColorBit);
        texture.SetPixels(pixels, pool, queue);
        texture.CreateTextureSampler();
        texture.CreateImageView();
        
        return texture;
    }
}

