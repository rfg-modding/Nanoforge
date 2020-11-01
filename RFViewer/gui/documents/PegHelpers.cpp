#include "PegHelpers.h"
#include "PegHelpers.h"
#include <DirectXTex.h>
#include "Log.h"
#include "util/StringHelpers.h"
#include "common/filesystem/Path.h"

namespace PegHelpers
{
    void ExportAll(PegFile10& peg, const string& gpuFilePath, const string& exportFolderPath)
    {
        for (u32 i = 0; i < peg.Entries.size(); i++)
        {
            ExportSingle(peg, gpuFilePath, i, exportFolderPath);
        }
    }

    void ExportSingle(PegFile10& peg, const string& gpuFilePath, u32 entryIndex, const string& exportFolderPath)
    {
        //Get entry ref and create reader for gpu file
        PegEntry10& entry = peg.Entries[entryIndex];
        BinaryReader gpuFile(gpuFilePath);

        //First make sure we have the raw data for the target entry
        peg.ReadTextureData(gpuFile, entry);

        //Now we need to tell DirectXTex how to output this data
        DXGI_FORMAT format = PegFormatToDxgiFormat(entry.BitmapFormat);
        DirectX::Image image;
        image.width = entry.Width;
        image.height = entry.Height;
        image.format = format;
        image.rowPitch = CalcRowPitch(format, entry.Width, entry.Height);
        image.slicePitch = entry.RawData.size_bytes();
        image.pixels = entry.RawData.data();

        string tempPath = exportFolderPath + Path::GetFileNameNoExtension(entry.Name) + ".dds";
        const wchar_t* pathWide = WidenCString(tempPath.c_str());
        //HRESULT result = SaveToWICFile(image, DirectX::WIC_FLAGS_NONE, DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG), pathWide);
        HRESULT result = DirectX::SaveToDDSFile(image, DirectX::DDS_FLAGS::DDS_FLAGS_NONE, pathWide);
        if (FAILED(result))
            Log->error("Error when saving \"{}\" to path \"{}\". Result: {:x}", entry.Name, exportFolderPath, (u32)result);

        delete pathWide;
    }

    DXGI_FORMAT PegFormatToDxgiFormat(PegFormat input)
    {
        if (input == PegFormat::PC_DXT1)
            return DXGI_FORMAT_BC1_UNORM; //DXT1
        else if (input == PegFormat::PC_DXT3)
            return DXGI_FORMAT_BC2_UNORM; //DXT2/3
        else if (input == PegFormat::PC_DXT5)
            return DXGI_FORMAT_BC3_UNORM; //DXT4/5
        else if (input == PegFormat::PC_8888)
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        else
            THROW_EXCEPTION("Unknown or unsupported format '{}' passed to PegFormatToDxgiFormat()", input);
    }

    string PegFormatToString(PegFormat input)
    {
        switch (input)
        {
        case PegFormat::None:
            return "None";
        case PegFormat::BM_1555:
            return "BM_1555";
        case PegFormat::BM_888:
            return "BM_888";
        case PegFormat::BM_8888:
            return "BM_8888";
        case PegFormat::PS2_PAL4:
            return "PS2_PAL4";
        case PegFormat::PS2_PAL8:
            return "PS2_PAL8";
        case PegFormat::PS2_MPEG32:
            return "PS2_MPEG32";
        case PegFormat::PC_DXT1:
            return "PC_DXT1";
        case PegFormat::PC_DXT3:
            return "PC_DXT3";
        case PegFormat::PC_DXT5:
            return "PC_DXT5";
        case PegFormat::PC_565:
            return "PC_565";
        case PegFormat::PC_1555:
            return "PC_1555";
        case PegFormat::PC_4444:
            return "PC_4444";
        case PegFormat::PC_888:
            return "PC_888";
        case PegFormat::PC_8888:
            return "PC_8888";
        case PegFormat::PC_16_DUDV:
            return "PC_16_DUDV";
        case PegFormat::PC_16_DOT3_COMPRESSED:
            return "PC_16_DOT3_COMPRESSED";
        case PegFormat::PC_A8:
            return "PC_A8";
        case PegFormat::XBOX2_DXN:
            return "XBOX2_DXN";
        case PegFormat::XBOX2_DXT3A:
            return "XBOX2_DXT3A";
        case PegFormat::XBOX2_DXT5A:
            return "XBOX2_DXT5A";
        case PegFormat::XBOX2_CTX1:
            return "XBOX2_CTX1";
        case PegFormat::PS3_DXT5N:
            return "PS3_DXT5N";
        default:
            return "Invalid peg format code";
        }
    }

    u32 CalcRowPitch(DXGI_FORMAT format, u32 width, u32 height)
    {
        if (format == DXGI_FORMAT_BC1_UNORM) //DXT1
            return 8 * (width / 4);
        else if (format == DXGI_FORMAT_BC2_UNORM) //DXT2/3
            return 16 * (width / 4);
        else if (format == DXGI_FORMAT_BC3_UNORM) //DXT4/5
            return 16 * (width / 4);
        else if (format == DXGI_FORMAT_R8G8B8A8_UNORM) //RGBA, 8 bits per pixel
            return 4 * width;
    }
}