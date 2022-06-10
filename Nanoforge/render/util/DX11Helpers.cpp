#include "DX11Helpers.h"
#include "common/filesystem/File.h"
#include "common/string/String.h"
#include "BuildConfig.h"
#include "Log.h"
#include <filesystem>
#include <d3dcompiler.h>
#include <d3d11.h>

//Custom include handler that records shader dependencies. Used by shader live reload code to reload shader if any included files change
class D3DIncludeHandler : public ID3DInclude
{
public:
    std::vector<string> IncludePaths = {};
    std::vector<std::filesystem::file_time_type> WriteTimes = {};

    virtual HRESULT __stdcall Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID* ppData, UINT* pBytes) override
    {
        try
        {
            //Record path & last write time of include file for live reload dependency tracking
            string includeShaderPath = fmt::format("{}/shaders/{}", BuildConfig::AssetFolderPath, pFileName);
            IncludePaths.push_back(includeShaderPath);
            WriteTimes.push_back(std::filesystem::last_write_time(includeShaderPath));

            //Load shader file into memory. ::Close() handles deleting it
            std::span<u8> bytes = File::ReadAllBytesSpan(includeShaderPath);
            *ppData = (void*)bytes.data();
            *pBytes = (u32)bytes.size_bytes();
        }
        catch (std::exception& ex)
        {
            LOG_ERROR("Failed to load shader include '{}'. Ex: '{}'", pFileName, ex.what());
            return E_FAIL;
        }
        return S_OK;
    }

    virtual HRESULT __stdcall Close(LPCVOID pData) override
    {
        u8* data = (u8*)pData;
        delete[] data;
        return S_OK;
    }

};

HRESULT CompileShaderFromFile(const WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3D10Blob** ppBlobOut, std::vector<string>* includePaths, std::vector<std::filesystem::file_time_type>* includeWriteTimes)
{
    D3DIncludeHandler includeHandler;

    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef ENABLE_D3D11_DEBUG_FEATURES
    //Embed debug info into shaders and don't optimize them for best debugging experience
    dwShaderFlags |= D3DCOMPILE_DEBUG;
    dwShaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    
    ID3DBlob* pErrorBlob = nullptr;
    HRESULT result = D3DCompileFromFile(szFileName, nullptr, &includeHandler, szEntryPoint, szShaderModel, dwShaderFlags, 0, ppBlobOut, &pErrorBlob);
    if (FAILED(result))
    {
        const char* error = (const char*)pErrorBlob->GetBufferPointer();
        if (pErrorBlob)
        {
            Log->error("Shader compiler error message: \"{}\"", error);
            OutputDebugStringA(reinterpret_cast<const char*>(pErrorBlob->GetBufferPointer()));
            pErrorBlob->Release();
        }
        return result;
    }
    if (pErrorBlob)
        pErrorBlob->Release();

    if (includePaths && includeWriteTimes)
    {
        *includePaths = std::vector<string>(includeHandler.IncludePaths);
        *includeWriteTimes = std::vector<std::filesystem::file_time_type>(includeHandler.WriteTimes);
    }

    return S_OK;
}

void SetDebugName(ID3D11DeviceChild* child, const std::string& name)
{
    if (child != nullptr)
        child->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<u32>(name.size()), name.c_str());
}