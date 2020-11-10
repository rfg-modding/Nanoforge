#include "Scene.h"
#include "render/util/DX11Helpers.h"
#include <filesystem>
#include "Log.h"
#include "util/StringHelpers.h"

//Todo: Stick this in a debug namespace
void SetDebugName(ID3D11DeviceChild* child, const std::string& name)
{
    if (child != nullptr)
        child->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<u32>(name.size()), name.c_str());
}

void Scene::Init(ComPtr<ID3D11Device> d3d11Device, ComPtr<ID3D11DeviceContext> d3d11Context)
{
    d3d11Device_ = d3d11Device;
    d3d11Context_ = d3d11Context;

    InitInternal();
    InitRenderTarget();

    //Create buffer for per object constants
    perObjectBuffer_.Create(d3d11Device_, sizeof(PerObjectConstants), D3D11_BIND_CONSTANT_BUFFER);
}

void Scene::SetShader(const string& path)
{
    shader_.Load(path, d3d11Device_);
}

void Scene::SetVertexLayout(const std::vector<D3D11_INPUT_ELEMENT_DESC>& layout)
{
    auto pVSBlob = shader_.GetVertexShaderBytes();

    //Create the input layout
    if (FAILED(d3d11Device_->CreateInputLayout(layout.data(), layout.size(), pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), vertexLayout_.GetAddressOf())))
        THROW_EXCEPTION("Failed to create terrain vertex layout");

    //Set the input layout
    d3d11Context_->IASetInputLayout(vertexLayout_.Get());
}

void Scene::Draw()
{
    //Reload shaders if necessary
    shader_.TryReload();

    //Set render target and clear it
    d3d11Context_->ClearRenderTargetView(sceneViewTexture_.GetRenderTargetView(), reinterpret_cast<const float*>(&ClearColor));
    d3d11Context_->ClearDepthStencilView(depthBufferTexture_.GetDepthStencilView(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    d3d11Context_->OMSetRenderTargets(1, sceneViewTexture_.GetRenderTargetViewPP(), depthBufferTexture_.GetDepthStencilView());
    d3d11Context_->RSSetViewports(1, &sceneViewport_);

    //Update per-frame constant buffer
    perFrameStagingBuffer_.ViewPos = Cam.Position();
    perFrameBuffer_.SetData(d3d11Context_, &perFrameStagingBuffer_);
    d3d11Context_->PSSetConstantBuffers(0, 1, perFrameBuffer_.GetAddressOf());

    //Draw render objects
    d3d11Context_->IASetInputLayout(vertexLayout_.Get());
    d3d11Context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    shader_.Bind(d3d11Context_);
    d3d11Context_->VSSetConstantBuffers(0, 1, perObjectBuffer_.GetAddressOf());
    for (auto& renderObject : Objects)
        renderObject.Draw(d3d11Context_, perObjectBuffer_, Cam);
}

void Scene::HandleResize(u32 windowWidth, u32 windowHeight)
{
    if (windowWidth != sceneViewWidth_ || windowHeight != sceneViewHeight_)
    {
        sceneViewWidth_ = windowWidth;
        sceneViewHeight_ = windowHeight;
        
        //Recreate scene view resources with new size
        InitInternal();
        CreateDepthBuffer();

        sceneViewport_.TopLeftX = 0.0f;
        sceneViewport_.TopLeftY = 0.0f;
        sceneViewport_.Width = static_cast<f32>(sceneViewWidth_);
        sceneViewport_.Height = static_cast<f32>(sceneViewHeight_);
        sceneViewport_.MinDepth = 0.0f;
        sceneViewport_.MaxDepth = 1.0f;
        Cam.HandleResize({ (f32)sceneViewWidth_, (f32)sceneViewHeight_ });
    }
}

void Scene::InitInternal()
{
    //Create texture and map a render target and shader resource view to it
    DXGI_FORMAT sceneViewFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
    sceneViewTexture_.Create(d3d11Device_, sceneViewWidth_, sceneViewHeight_, sceneViewFormat, (D3D11_BIND_FLAG)(D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE));
    sceneViewTexture_.CreateRenderTargetView(); //Allows us to use texture as a render target
    sceneViewTexture_.CreateShaderResourceView(); //Lets shaders view texture. Used by dear imgui to draw textures in gui
#ifdef DEBUG_BUILD
    SetDebugName(sceneViewTexture_.Get(), "sceneViewTexture_");
#endif

    //Create per-frame constant buffer
    perFrameBuffer_.Create(d3d11Device_, sizeof(PerFrameConstants), D3D11_BIND_CONSTANT_BUFFER);
}

void Scene::InitRenderTarget()
{
    CreateDepthBuffer();
    d3d11Context_->OMSetRenderTargets(1, sceneViewTexture_.GetRenderTargetViewPP(), depthBufferTexture_.GetDepthStencilView());
    d3d11Context_->RSSetViewports(1, &sceneViewport_);
}

void Scene::CreateDepthBuffer()
{
    //Create depth buffer texture and view
    depthBufferTexture_.Create(d3d11Device_, sceneViewWidth_, sceneViewHeight_, DXGI_FORMAT_D24_UNORM_S8_UINT, D3D11_BIND_DEPTH_STENCIL);
    depthBufferTexture_.CreateDepthStencilView();
}