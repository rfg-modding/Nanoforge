#pragma once
#include "common/Typedefs.h"
#include <dxgiformat.h>
#include <d3d11.h>
#include <vector>
#include <span>

class PegFile10;
class PegEntry10;
enum class PegFormat : u16;

namespace PegHelpers
{
    //Export all subtextures in a peg. You must call ::ReadAllTextureData() on the peg before exporting. If you get the peg from TextureIndex this is already done.
    bool ExportAll(PegFile10& peg, std::string_view exportFolderPath);
    //Export a single subtexture in a peg. You must call ::ReadTextureData() on the peg for the subtexture before exporting. If you get the peg from TextureIndex this is already done.
    bool ExportSingle(PegFile10& peg, std::string_view exportFolderPath, std::string_view subtextureName);
    bool ExportSingle(PegFile10& peg, std::string_view exportFolderPath, size_t subtextureIndex);

    //Import texture and replace provided entry with it's data
    void ImportTexture(PegFile10& peg, u32 targetIndex, std::string_view importFilePath);
    //Convert peg format to dxgi format
    DXGI_FORMAT PegFormatToDxgiFormat(PegFormat input, u16 flags);
    //Convert dxgi format to peg format
    PegFormat DxgiFormatToPegFormat(DXGI_FORMAT input);
    //Convert PegFormat enum to string
    string PegFormatToString(PegFormat input);
    //Calculate size of a row in bytes for an image based on it's dxgi format and dimensions
    u32 CalcRowPitch(DXGI_FORMAT format, u32 numPixels);
    //Calculate D3D11_SUBRESOURCE_DATA for each mip level of a peg subtexture
    std::vector<D3D11_SUBRESOURCE_DATA> CalcSubresourceData(PegEntry10& entry, std::span<u8> pixels);
}