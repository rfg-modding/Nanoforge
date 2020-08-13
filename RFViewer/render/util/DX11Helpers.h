#pragma once
#include <ext/WindowsWrapper.h>

#define ReleaseCOM(x) if(x) x->Release();

//Todo: Make this more advanced and provide option that doesn't use exceptions. See Im3d's dxAssert as a good example.
#define DxCheck(call, failureMessage) \
    if(FAILED(call)) \
        throw failureMessage; \

struct ID3D10Blob;
HRESULT CompileShaderFromFile(const WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3D10Blob** ppBlobOut, const char* defines = nullptr);