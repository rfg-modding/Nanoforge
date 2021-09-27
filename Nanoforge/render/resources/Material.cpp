#include "Material.h"
#include "application/Config.h"
#include "Log.h"

Material::Material(ComPtr<ID3D11Device> d3d11Device, Config* config, const string& shaderName, const std::vector<D3D11_INPUT_ELEMENT_DESC>& layout)
{
    Init(d3d11Device, config, shaderName, layout);
}

void Material::Init(ComPtr<ID3D11Device> d3d11Device, Config* config, const string& shaderName, const std::vector<D3D11_INPUT_ELEMENT_DESC>& layout)
{
    //Ensure shader config vars exist
    if (!config->Exists("Use geometry shaders"))
    {
        config->EnsureVariableExists("Use geometry shaders", ConfigType::Bool);
        auto useGeomShaders = config->GetVariable("Use geometry shaders");
        std::get<bool>(useGeomShaders->Value) = true;
        config->Save();
    }

    //Load shaders
    shader_.Load(shaderName, d3d11Device, config->GetBoolReadonly("Use geometry shaders").value());
    auto vsBytes = shader_.GetVertexShaderBytes();

    //Setup vertex input layout
    if (FAILED(d3d11Device->CreateInputLayout(layout.data(), layout.size(), vsBytes->GetBufferPointer(), vsBytes->GetBufferSize(), vertexLayout_.GetAddressOf())))
        THROW_EXCEPTION("Failed to create Material vertex input layout.");

    initialized_ = true;
}

void Material::Use(ComPtr<ID3D11DeviceContext> d3d11Context)
{
    d3d11Context->IASetInputLayout(vertexLayout_.Get());
    shader_.Bind(d3d11Context);
}

void Material::TryShaderReload()
{
    shader_.TryReload();
}
