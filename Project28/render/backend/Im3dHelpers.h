//A set of functions pulled directly from the Im3d dx11 example. Using them as is at first to keep things simple
//Todo: Eliminate these and use + expand shader loading functions already used by other parts of the renderer
#pragma once
#include <d3dcommon.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include "render/util/DX11Helpers.h"
#include "render/camera/Camera.h"
#include <vector>
#include <im3d.h>

void* MapBuffer(ID3D11DeviceContext* deviceContext, ID3D11Buffer* _buffer, D3D11_MAP _mapType)
{
    D3D11_MAPPED_SUBRESOURCE subRes;
    DxCheck(deviceContext->Map(_buffer, 0, _mapType, 0, &subRes), "Failed to map buffer");
    return subRes.pData;
}

void UnmapBuffer(ID3D11DeviceContext* deviceContext, ID3D11Buffer* _buffer)
{
    deviceContext->Unmap(_buffer, 0);
}

static const char* StripPath(const char* _path)
{
    int i = 0, last = 0;
    while (_path[i] != '\0') {
        if (_path[i] == '\\' || _path[i] == '/') {
            last = i + 1;
        }
        ++i;
    }
    return &_path[last];
}

static void Append(const char* _str, std::vector<char>& _out_)
{
    while (*_str)
    {
        _out_.push_back(*_str);
        ++_str;
    }
}
static void AppendLine(const char* _str, std::vector<char>& _out_)
{
    Append(_str, _out_);
    _out_.push_back('\n');
}

static bool LoadShader(const char* _path, const char* _defines, std::vector<char>& _out_)
{
    fprintf(stdout, "Loading shader: '%s'", StripPath(_path));
    if (_defines)
    {
        fprintf(stdout, " ");
        while (*_defines != '\0')
        {
            fprintf(stdout, " %s,", _defines);
            Append("#define ", _out_);
            AppendLine(_defines, _out_);
            _defines = strchr(_defines, 0);
            ++_defines;
        }
    }
    fprintf(stdout, "\n");

    FILE* fin = fopen(_path, "rb");
    if (!fin)
    {
        fprintf(stderr, "Error opening '%s'\n", _path);
        return false;
    }
    fseek(fin, 0, SEEK_END); // not portable but should work almost everywhere
    long fsize = ftell(fin);
    fseek(fin, 0, SEEK_SET);

    int srcbeg = _out_.size();
    _out_.resize(srcbeg + fsize, '\0');
    if (fread(_out_.data() + srcbeg, 1, fsize, fin) != fsize)
    {
        fclose(fin);
        fprintf(stderr, "Error reading '%s'\n", _path);
        return false;
    }
    fclose(fin);
    _out_.push_back('\0');

    return true;
}

ID3DBlob* LoadCompileShader(const char* _target, const char* _path, const char* _defines)
{
    std::vector<char> src;
    if (!LoadShader(_path, _defines, src))
    {
        return nullptr;
    }
    ID3DBlob* ret = nullptr;
    ID3DBlob* err = nullptr;
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;

    // D3DCompile is not portable - linking with d3dcompiler.lib introdices a dependency on d3dcompiler_XX.lib
    // \todo get a ptr to D3DCompile at runtime via LoadLibrary/GetProcAddress
    D3DCompile(src.data(), src.size(), nullptr, nullptr, nullptr, "main", _target, flags, 0, &ret, &err);
    if (ret == nullptr)
    {
        fprintf(stderr, "Error compiling '%s':\n\n", _path);
        if (err)
        {
            fprintf(stderr, (char*)err->GetBufferPointer());
            err->Release();
        }
    }

    return ret;
}