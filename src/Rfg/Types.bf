using Common;
using System;
using Nanoforge.App;
using Direct3D;
using Nanoforge.Render.Resources;
using System.Collections;
using RfgTools.Formats.Textures;
using System.Threading;
using Nanoforge.Misc;

namespace Nanoforge.Rfg
{
    //List of peg subtextures imported and stored in project buffers. Used to prevent repeat imports. Only one instance of this should exist
    //TODO: The key is the subtexture name so it doesn't handle two peg subtextures with the same name but different pixels correctly. This will need to be fixed when texture editing is supported.
    [ReflectAll]
    public class ImportedTextures : EditorObject
    {
        private append Monitor _newTextureLock;
        [EditorProperty]
        public List<ProjectTexture> Textures = new .() ~delete _;

        public Result<ProjectTexture> GetTexture(StringView name)
        {
            ScopedLock!(_newTextureLock);
            for (ProjectTexture texture in Textures)
            {
                if (texture.Name == name)
                {
                    return texture;
                }
            }
            return .Err;
        }

        public void AddTexture(ProjectTexture texture)
        {
            ScopedLock!(_newTextureLock);
            Textures.Add(texture);
        }
    }

    //Texture contained by the ProjectDB
    [ReflectAll]
    public class ProjectTexture : EditorObject
    {
        //TODO: Add ProjectDB support for ProjectBuffer
        //TODO: Make this an editor property
        public ProjectBuffer Data;
        [EditorProperty]
        public DXGI_FORMAT Format;
        [EditorProperty]
        public u32 Width;
        [EditorProperty]
        public u32 Height;
        [EditorProperty]
        public u32 NumMipLevels;

        public Result<Texture2D> CreateRenderTexture(ID3D11Device* device)
        {
            if (Data == null)
		        return .Err;

            List<u8> pixels = Data.Load().GetValueOrDefault(null);
            defer { DeleteIfSet!(pixels); }
            if (pixels == null)
                return .Err;

            List<D3D11_SUBRESOURCE_DATA> subresourceData = CalcSubresourceData(pixels, .. scope .());
            Texture2D renderTexture = new .(Name);
            if (renderTexture.Init(device, Width, Height, Format, .SHADER_RESOURCE, subresourceData.Ptr, NumMipLevels) case .Err)
            {
                delete renderTexture;
				return .Err;
			}
            renderTexture.CreateShaderResourceView();
            renderTexture.CreateSampler();

            return renderTexture;
        }

        public void CalcSubresourceData(Span<u8> pixels, List<D3D11_SUBRESOURCE_DATA> subresources)
        {
            int dataOffset = 0;
            u32 mipWidth = Width;
            u32 mipHeight = Height;
            for (int i in 0 ..< NumMipLevels)
            {
                D3D11_SUBRESOURCE_DATA subresource = .();
                subresource.pSysMem = pixels.Ptr + dataOffset;
                subresource.SysMemSlicePitch = 0;
                subresource.SysMemPitch = CalcRowPitch(Format, mipWidth);

                //Note: Only other texture format known to be used by the game is PC_8888, which so far doesn't ever have multiple mip levels
                if (Format == DXGI_FORMAT.BC1_UNORM || Format == DXGI_FORMAT.BC1_UNORM_SRGB) //DXT1
                    dataOffset += 8 * (mipWidth * mipHeight / (4 * 4)); //8 bytes per 4x4 pixel block
                if (Format == DXGI_FORMAT.BC2_UNORM || Format == DXGI_FORMAT.BC2_UNORM_SRGB) //DXT2/3
                    dataOffset += 16 * (mipWidth * mipHeight / (4 * 4)); //16 bytes per 4x4 pixel block
                if (Format == DXGI_FORMAT.BC3_UNORM || Format == DXGI_FORMAT.BC3_UNORM_SRGB) //DXT4/5
                    dataOffset += 16 * (mipWidth * mipHeight / (4 * 4)); //16 bytes per 4x4 pixel block

                mipWidth /= 2;
                mipHeight /= 2;

                subresources.Add(subresource);
            }
        }

        public static u32 CalcRowPitch(DXGI_FORMAT format, u32 numPixels)
        {
            if (format == DXGI_FORMAT.BC1_UNORM || format == DXGI_FORMAT.BC1_UNORM_SRGB) //DXT1
                return 8 * (numPixels / 4);
            else if (format == DXGI_FORMAT.BC2_UNORM || format == DXGI_FORMAT.BC2_UNORM_SRGB) //DXT2/3
                return 16 * (numPixels / 4);
            else if (format == DXGI_FORMAT.BC3_UNORM || format == DXGI_FORMAT.BC3_UNORM_SRGB) //DXT4/5
                return 16 * (numPixels / 4);
            else if (format == DXGI_FORMAT.R8G8B8A8_UNORM || format == DXGI_FORMAT.R8G8B8A8_UNORM_SRGB) //RGBA, 8 bits per pixel
                return 4 * numPixels;
            else
                return 0;
        }

        public static Result<DXGI_FORMAT> PegFormatToDxgiFormat(PegFormat input, u16 flags)
        {
            bool srgb = (flags & 512) != 0;
            if (input == .PC_DXT1)
                return srgb ? DXGI_FORMAT.BC1_UNORM_SRGB : DXGI_FORMAT.BC1_UNORM; //DXT1
            else if (input == .PC_DXT3)
                return srgb ? DXGI_FORMAT.BC2_UNORM_SRGB : DXGI_FORMAT.BC2_UNORM; //DXT2/3
            else if (input == .PC_DXT5)
                return srgb ? DXGI_FORMAT.BC3_UNORM_SRGB : DXGI_FORMAT.BC3_UNORM; //DXT4/5
            else if (input == .PC_8888)
                return srgb ? DXGI_FORMAT.R8G8B8A8_UNORM_SRGB : DXGI_FORMAT.R8G8B8A8_UNORM;
            else
                return .Err;
        }
    }
}