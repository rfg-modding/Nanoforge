#include "Shader.h"
#include "BuildConfig.h"
#include "Log.h"
#include "util/StringHelpers.h"
#include "render/util/DX11Helpers.h"
#include "common/filesystem/Path.h"
#include "common/filesystem/File.h"
#include "common/string/String.h"

void Shader::Load(std::string_view shaderName, ComPtr<ID3D11Device> d3d11Device, bool useGeometryShaders)
{
    //Store args in member variables for reloads
    shaderPath_ = fmt::format("{}{}", BuildConfig::ShaderPath, shaderName);
    d3d11Device_ = d3d11Device;
    shaderWriteTime_ = std::filesystem::last_write_time(shaderPath_);
    useGeometryShaders_ = useGeometryShaders;

    //Compile & create terrain vertex and pixel shaders. Using smart pointers for all heap data so they auto free
    ComPtr<ID3DBlob> pPSBlob = nullptr;
    std::unique_ptr<wchar_t[]> shaderPathWide = WidenCString(shaderPath_.c_str());

    ComPtr<ID3D11VertexShader> vertexShader = nullptr;
    ComPtr<ID3D11PixelShader> pixelShader = nullptr;
    ComPtr<ID3D11GeometryShader> geometryShader = nullptr;
    ComPtr<ID3DBlob> pVSBlob = nullptr;
    ComPtr<ID3DBlob> pGSBlob = nullptr;

    //Get data about included files when compiling vertex shader. Can ignore other times since all Nanoforge shaders are merged into one file
    includePaths_.clear();
    includeWriteTimes_.clear();

    //Compile the vertex shader
    if (FAILED(CompileShaderFromFile(shaderPathWide.get(), "VS", "vs_4_0", pVSBlob.GetAddressOf(), &includePaths_, &includeWriteTimes_)))
    {
        LOG_ERROR("Failed to compile vertex shader in {}", Path::GetFileName(shaderPath_));
        return;
    }
    if (FAILED(d3d11Device_->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, vertexShader.GetAddressOf())))
    {
        LOG_ERROR("Failed to create vertex shader in {}", Path::GetFileName(shaderPath_));
        return;
    }

    //Compile the pixel shader
    if (FAILED(CompileShaderFromFile(shaderPathWide.get(), "PS", "ps_4_0", pPSBlob.GetAddressOf())))
    {
        LOG_ERROR("Failed to compile pixel shader in {}", Path::GetFileName(shaderPath_));
        return;
    }
    if (FAILED(d3d11Device_->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, pixelShader.GetAddressOf())))
    {
        LOG_ERROR("Failed to create pixel shader in {}", Path::GetFileName(shaderPath_));
        return;
    }

    //Check if the shader file contains the string "void GS(". If that's found attempt to load a geometry shader
    bool geomShaderFound = String::Contains(File::ReadToString(shaderPath_), "GS") && useGeometryShaders;
    if (geomShaderFound)
    {
        //Compile geometry shader if one was found. Still continues if it fails to compile since geometry shaders are optional
        if (FAILED(CompileShaderFromFile(shaderPathWide.get(), "GS", "gs_4_0", pGSBlob.GetAddressOf())))
        {
            LOG_ERROR("Failed to compile geometry shader in {}", Path::GetFileName(shaderPath_));
        }
        else if (FAILED(d3d11Device_->CreateGeometryShader(pGSBlob->GetBufferPointer(), pGSBlob->GetBufferSize(), nullptr, geometryShader.GetAddressOf())))
        {
            geomShaderFound = false;
            LOG_ERROR("Failed to create geometry shader in {}", Path::GetFileName(shaderPath_));
        }
    }

    vertexShader_.Reset();
    pixelShader_.Reset();
    pVSBlob_.Reset();
    vertexShader_ = vertexShader;
    pixelShader_ = pixelShader;
    pVSBlob_ = pVSBlob;
    if (geomShaderFound)
    {
        geometryShader_.Reset();
        geometryShader_ = geometryShader;
    }
    Log->info("\"{}\" reloaded.", Path::GetFileName(shaderPath_));
}

void Shader::TryReload()
{
    if (NeedsReload())
    {
        //Wait for a moment as a quickfix to a crash that happens when we read the shader as it's being saved
        //Later note: LOL. The "quickfix" is still here after almost 2 years. Not gonna bother trying anything else since it already works, and shader reload is very infrequent and only for dev purposes.
        Log->info("Reloading shader \"{}\"", Path::GetFileName(shaderPath_));
        Sleep(250);
        Load(Path::GetFileName(shaderPath_), d3d11Device_, useGeometryShaders_);
    }
}

void Shader::Bind(ComPtr<ID3D11DeviceContext> d3d11Context)
{
    d3d11Context->VSSetShader(vertexShader_.Get(), nullptr, 0);
    d3d11Context->PSSetShader(pixelShader_.Get(), nullptr, 0);
    if(useGeometryShaders_)
        d3d11Context->GSSetShader(geometryShader_.Get(), nullptr, 0);
}

bool Shader::NeedsReload()
{
    //Reload shader if file has been written to since last check
    if (shaderWriteTime_ != std::filesystem::last_write_time(shaderPath_))
        return true;

    //Reload shader if any included files have changed
    for (size_t i = 0; i < includePaths_.size(); i++)
        if (includeWriteTimes_[i] != std::filesystem::last_write_time(includePaths_[i]))
            return true;

    return false;
}
