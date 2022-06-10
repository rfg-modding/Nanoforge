#pragma once
#include <ext/WindowsWrapper.h>
#include <filesystem>
#include "Log.h"

struct ID3D11DeviceChild;

#define ReleaseCOM(x) if(x) x->Release();

#define DxCheck(call, failureMessage) \
{ \
    HRESULT result = call; \
    if (result != S_OK) \
        THROW_EXCEPTION("DxCheck failed. Result: {}, Message: {}", result, failureMessage); \
} \


struct ID3D10Blob;
HRESULT CompileShaderFromFile(const WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3D10Blob** ppBlobOut, std::vector<string>*includePaths = nullptr, std::vector<std::filesystem::file_time_type>*includeWriteTimes = nullptr);
void SetDebugName(ID3D11DeviceChild* child, const std::string& name);