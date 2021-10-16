#pragma once
#include "common/Typedefs.h"
#include "render/camera/Camera.h"
#include <d3d11.h>
#include <filesystem>
#include <wrl.h>
using Microsoft::WRL::ComPtr;

//Wraps ID3D11VertexShader and ID3D11PixelShader. Has functions for loading/reloading shaders from source
class Shader
{
public:
    //Attempts to load a shader pair from the provided path
    void Load(std::string_view shaderName, ComPtr<ID3D11Device> d3d11Device, bool useGeometryShaders);
    //Reloads shader if it's file changed and enough time has elapsed since the last reload
    void TryReload();
    //Bind shader to current context
    void Bind(ComPtr<ID3D11DeviceContext> d3d11Context);
    //Get vertex shader bytes. Needed to set vertex input layout
    ComPtr<ID3DBlob> GetVertexShaderBytes() { return pVSBlob_; }

private:
    std::filesystem::file_time_type shaderWriteTime_;
    ComPtr<ID3D11VertexShader> vertexShader_ = nullptr;
    ComPtr<ID3D11PixelShader> pixelShader_ = nullptr;
    ComPtr<ID3D11GeometryShader> geometryShader_ = nullptr;
    ComPtr<ID3D11Device> d3d11Device_ = nullptr;
    ComPtr<ID3DBlob> pVSBlob_ = nullptr;
    string shaderPath_;
    bool useGeometryShaders_ = false;
};