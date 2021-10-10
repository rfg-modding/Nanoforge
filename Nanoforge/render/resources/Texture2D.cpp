#include "Texture2D.h"
#include "render/util/DX11Helpers.h"

void Texture2D::Create(ComPtr<ID3D11Device> d3d11Device, u32 width, u32 height, DXGI_FORMAT format, D3D11_BIND_FLAG bindFlags, D3D11_SUBRESOURCE_DATA* data, u32 numMipLevels)
{
    d3d11Device_ = d3d11Device;
    format_ = format;
    mipLevels_ = numMipLevels;

    //Clear any existing resources. This way you can call ::Create repeatedly to resize or recreate the texture
    depthStencilView_.Reset();
    shaderResourceView_.Reset();
    renderTargetView_.Reset();
    texture_.Reset();

    D3D11_TEXTURE2D_DESC textureDesc;
    ZeroMemory(&textureDesc, sizeof(D3D11_TEXTURE2D_DESC));
    textureDesc.Width = width;
    textureDesc.Height = height;
    textureDesc.MipLevels = numMipLevels;
    textureDesc.ArraySize = 1;
    textureDesc.Format = format_;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = bindFlags;
    textureDesc.CPUAccessFlags = 0;
    textureDesc.MiscFlags = 0;
    HRESULT result = d3d11Device->CreateTexture2D(&textureDesc, data, texture_.GetAddressOf());
    if (!texture_)
        THROW_EXCEPTION("Failed to create Texture2D. Result: {}", result);

#ifdef DEBUG_BUILD
    SetDebugName(texture_.Get(), Name);
#endif
}

void Texture2D::CreateRenderTargetView()
{
    //Release existing resources
    renderTargetView_.Reset();

    //Describe render target view. This is used render to a texture
    D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
    renderTargetViewDesc.Format = format_;
    renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    renderTargetViewDesc.Texture2D.MipSlice = 0;

    //Create render target view and map to texture_
    if (FAILED(d3d11Device_->CreateRenderTargetView(texture_.Get(), &renderTargetViewDesc, renderTargetView_.GetAddressOf())))
        THROW_EXCEPTION("Failed to create render target view from Texture2D.");
}

void Texture2D::CreateShaderResourceView()
{
    //Release existing resources
    shaderResourceView_.Reset();

    //Create shader resource for dear imgui to use
    D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;
    shaderResourceViewDesc.Format = format_;
    shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
    shaderResourceViewDesc.Texture2D.MipLevels = mipLevels_;

    //Create the shader resource view
    if(FAILED(d3d11Device_->CreateShaderResourceView(texture_.Get(), &shaderResourceViewDesc, shaderResourceView_.GetAddressOf())))
        THROW_EXCEPTION("Failed to create shader resource view from Texture2D.");
}

void Texture2D::CreateDepthStencilView()
{
    //Release existing resources
    depthStencilView_.Reset();

    if (FAILED(d3d11Device_->CreateDepthStencilView(texture_.Get(), 0, depthStencilView_.GetAddressOf())))
        THROW_EXCEPTION("Failed to create depth stencil view from Texture2D.");
}

void Texture2D::CreateSampler()
{
    //Setup sampler description
    D3D11_SAMPLER_DESC sampleDesc;
    ZeroMemory(&sampleDesc, sizeof(sampleDesc));
    sampleDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampleDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampleDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampleDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampleDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampleDesc.MinLOD = 0;
    sampleDesc.MaxLOD = D3D11_FLOAT32_MAX;

    //Create the sampler
    if (FAILED(d3d11Device_->CreateSamplerState(&sampleDesc, samplerState_.GetAddressOf())))
        THROW_EXCEPTION("Failed to create depth stencil view  from Texture2D.");
}

void Texture2D::Bind(ComPtr<ID3D11DeviceContext> d3d11Context, u32 index)
{
    d3d11Context->PSSetShaderResources(index, 1, shaderResourceView_.GetAddressOf());
    d3d11Context->PSSetSamplers(index, 1, samplerState_.GetAddressOf());
}