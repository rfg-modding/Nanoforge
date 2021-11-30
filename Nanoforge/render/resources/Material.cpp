#include "Material.h"
#include "application/Config.h"
#include "Log.h"

CVar CVar_UseGeometryShaders("Use geometry shaders", ConfigType::Bool,
    "If enabled geometry shaders are used to draw lines in the 3D map view. If this is disabled lines will be thinner and harder to read. Toggleable since some older computers might not support them.",
    ConfigValue(true), //Default value
    true  //ShowInSettings
);

Material::Material(ComPtr<ID3D11Device> d3d11Device, const string& shaderName, const std::vector<D3D11_INPUT_ELEMENT_DESC>& layout)
{
    Init(d3d11Device, shaderName, layout);
}

void Material::Init(ComPtr<ID3D11Device> d3d11Device, const string& shaderName, const std::vector<D3D11_INPUT_ELEMENT_DESC>& layout)
{
    //Load shaders
    shader_.Load(shaderName, d3d11Device, CVar_UseGeometryShaders.Get<bool>());
    auto vsBytes = shader_.GetVertexShaderBytes();

    //Setup vertex input layout
    if (FAILED(d3d11Device->CreateInputLayout(layout.data(), (u32)layout.size(), vsBytes->GetBufferPointer(), (u32)vsBytes->GetBufferSize(), vertexLayout_.GetAddressOf())))
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
