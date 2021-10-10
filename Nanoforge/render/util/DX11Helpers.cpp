#include "DX11Helpers.h"
#include "Log.h"
#include <d3dcompiler.h>
#include <d3d11.h>
#include <iostream>

HRESULT CompileShaderFromFile(const WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3D10Blob** ppBlobOut, const char* defines)
{
    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef DEBUG_BUILD
    //Embed debug info into shaders and don't optimize them for best debugging experience
    dwShaderFlags |= D3DCOMPILE_DEBUG;
    dwShaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ID3DBlob* pErrorBlob = nullptr;
    HRESULT result = D3DCompileFromFile(szFileName, nullptr, nullptr, szEntryPoint, szShaderModel, dwShaderFlags, 0, ppBlobOut, &pErrorBlob);
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

    return S_OK;
}

void SetDebugName(ID3D11DeviceChild* child, const std::string& name)
{
    if (child != nullptr)
        child->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<u32>(name.size()), name.c_str());
}