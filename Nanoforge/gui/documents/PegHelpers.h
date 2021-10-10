#pragma once
#include "common/Typedefs.h"
#include "RfgTools++/formats/textures/PegFile10.h"
#include <dxgiformat.h>
#include <d3d11.h>
#include <vector>

//Todo: Move this into RfgTools++. Not done yet since we're relying on DirectXTex for some image import/export work
//Todo: and I don't like the idea of having a file handling lib (RfgTools++) be dependent on DX11.

namespace PegHelpers
{
    //Export all entries from peg
    void ExportAll(PegFile10& peg, const string& gpuFilePath, const string& exportFolderPath);
    //Export all entries from peg
    void ExportAll(const string& cpuFilePath, const string& gpuFilePath, const string& exportFolderPath);
    //Export single entry from peg
    void ExportSingle(PegFile10& peg, const string& gpuFilePath, u32 entryIndex, const string& exportFolderPath);
    //Export single entry from peg
    void ExportSingle(const string& cpuFilePath, const string& gpuFilePath, u32 entryIndex, const string& exportFolderPath);
    //Export single entry from peg
    void ExportSingle(const string& cpuFilePath, const string& gpuFilePath, const string& entryName, const string& exportFolderPath);

    //Import texture and replace provided entry with it's data
    void ImportTexture(PegFile10& peg, u32 targetIndex, const string& importFilePath);
    //Convert peg format to dxgi format
    DXGI_FORMAT PegFormatToDxgiFormat(PegFormat input, u16 flags);
    //Convert dxgi format to peg format
    PegFormat DxgiFormatToPegFormat(DXGI_FORMAT input);
    //Convert PegFormat enum to string
    string PegFormatToString(PegFormat input);
    //Calculate size of a row in bytes for an image based on it's dxgi format and dimensions
    u32 CalcRowPitch(DXGI_FORMAT format, u32 numPixels);
    //Calculate D3D11_SUBRESOURCE_DATA for each mip level of a peg subtexture
    std::vector<D3D11_SUBRESOURCE_DATA> CalcSubresourceData(PegEntry10& entry, u16 pegFlags, std::span<u8> pixels);
}