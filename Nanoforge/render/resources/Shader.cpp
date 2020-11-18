#include "Shader.h"
#include "Log.h"
#include "util/StringHelpers.h"
#include "render/util/DX11Helpers.h"
#include "common/filesystem/Path.h"

void Shader::Load(const string& shaderPath, ComPtr<ID3D11Device> d3d11Device)
{
    //Store args in member variables for reloads
    shaderPath_ = shaderPath;
    d3d11Device_ = d3d11Device;
    shaderWriteTime_ = std::filesystem::last_write_time(shaderPath_);

    //Compile & create terrain vertex and pixel shaders. Using smart pointers for all heap data so they auto free
    ComPtr<ID3DBlob> pPSBlob = nullptr;
    std::unique_ptr<wchar_t[]> shaderPathWide = WidenCString(shaderPath_.c_str());

    //Compile the vertex shader
    if (FAILED(CompileShaderFromFile(shaderPathWide.get(), "VS", "vs_4_0", pVSBlob_.GetAddressOf())))
        THROW_EXCEPTION("Failed to compile vertex shader!");
    if (FAILED(d3d11Device_->CreateVertexShader(pVSBlob_->GetBufferPointer(), pVSBlob_->GetBufferSize(), nullptr, vertexShader_.GetAddressOf())))
        THROW_EXCEPTION("Failed to create vertex shader!");

    //Compile the pixel shader
    if (FAILED(CompileShaderFromFile(shaderPathWide.get(), "PS", "ps_4_0", pPSBlob.GetAddressOf())))
        THROW_EXCEPTION("Failed to compile pixel shader!");
    if (FAILED(d3d11Device_->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, pixelShader_.GetAddressOf())))
        THROW_EXCEPTION("Failed to create pixel shader!");
}

void Shader::TryReload()
{
    //Reload shader if file has been written to since last check
    if (shaderWriteTime_ == std::filesystem::last_write_time(shaderPath_))
        return;
    
    //Release current resources and reload
    vertexShader_.Reset();
    pixelShader_.Reset();
    pVSBlob_.Reset();

    //Wait for a moment as a quickfix to a crash that happens when we read the shader as it's being saved
    Log->info("Reloading shader \"{}\"", Path::GetFileName(shaderPath_));
    Sleep(250);
    Load(shaderPath_, d3d11Device_);
    Log->info("Shaders reloaded.");
}

void Shader::Bind(ComPtr<ID3D11DeviceContext> d3d11Context)
{
    d3d11Context->VSSetShader(vertexShader_.Get(), nullptr, 0);
    d3d11Context->PSSetShader(pixelShader_.Get(), nullptr, 0);
}
