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
    InitPrimitiveState();
    InitRenderTarget();

    //Create buffer for per object constants
    perObjectBuffer_.Create(d3d11Device_, sizeof(PerObjectConstants), D3D11_BIND_CONSTANT_BUFFER);
}

void Scene::SetShader(const string& path)
{
    shader_.Load(path, d3d11Device_);
    shaderSet_ = true;
}

void Scene::SetVertexLayout(const std::vector<D3D11_INPUT_ELEMENT_DESC>& layout)
{
    auto pVSBlob = shader_.GetVertexShaderBytes();

    //Create the input layout
    if (FAILED(d3d11Device_->CreateInputLayout(layout.data(), layout.size(), pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), vertexLayout_.GetAddressOf())))
        THROW_EXCEPTION("Failed to create scene vertex layout");

    //Set the input layout
    d3d11Context_->IASetInputLayout(vertexLayout_.Get());
    vertexLayoutSet_ = true;
}

void Scene::Draw(f32 deltaTime)
{
    //Don't draw scene if critical data not set
    if (!shaderSet_ || !vertexLayoutSet_)
        return;

    //Reload shaders if necessary
    shader_.TryReload();
    linelistShader_.TryReload();

    //Set render target and clear it
    d3d11Context_->ClearRenderTargetView(sceneViewTexture_.GetRenderTargetView(), reinterpret_cast<const float*>(&ClearColor));
    d3d11Context_->ClearDepthStencilView(depthBufferTexture_.GetDepthStencilView(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    d3d11Context_->OMSetRenderTargets(1, sceneViewTexture_.GetRenderTargetViewPP(), depthBufferTexture_.GetDepthStencilView());
    d3d11Context_->RSSetViewports(1, &sceneViewport_);
    d3d11Context_->RSSetState(meshRasterizerState_.Get());

    //Update per-frame constant buffer
    perFrameStagingBuffer_.Time += deltaTime;
    perFrameStagingBuffer_.ViewPos = Cam.Position();
    perFrameStagingBuffer_.ViewportDimensions = Vec2{ (f32)sceneViewWidth_, (f32)sceneViewHeight_ };
    perFrameBuffer_.SetData(d3d11Context_, &perFrameStagingBuffer_);
    d3d11Context_->PSSetConstantBuffers(0, 1, perFrameBuffer_.GetAddressOf());

    //Draw render objects
    d3d11Context_->IASetInputLayout(vertexLayout_.Get());
    d3d11Context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    //Draw all render objects
    shader_.Bind(d3d11Context_);
    d3d11Context_->VSSetConstantBuffers(0, 1, perObjectBuffer_.GetAddressOf());
    for (auto& renderObject : Objects)
        renderObject.Draw(d3d11Context_, perObjectBuffer_, Cam);



    //Prepare state to render primitives
    d3d11Context_->RSSetState(primitiveRasterizerState_.Get());
    d3d11Context_->IASetInputLayout(linelistVertexLayout_.Get());
    d3d11Context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    linelistShader_.Bind(d3d11Context_);
    
    //Update primitive vertex buffers if necessary
    u32 linelistVertexStride = sizeof(ColoredVertex);
    u32 vertexOffset = 0;
    if (primitiveBufferNeedsUpdate_)
    {
        lineVertexBuffer_.ResizeIfNeeded(d3d11Device_, lineVertices_.size() * linelistVertexStride);
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        ZeroMemory(&mappedResource, sizeof(D3D11_MAPPED_SUBRESOURCE));
        d3d11Context_->Map(lineVertexBuffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
        memcpy(mappedResource.pData, lineVertices_.data(), lineVertices_.size() * linelistVertexStride);
        d3d11Context_->Unmap(lineVertexBuffer_.Get(), 0);
        
        numLineVertices_ = lineVertices_.size();
        lineVertices_.clear();
        primitiveBufferNeedsUpdate_ = false;
    }

    d3d11Context_->IASetVertexBuffers(0, 1, lineVertexBuffer_.GetAddressOf(), &linelistVertexStride, &vertexOffset);

    //Shader constants for all primitives
    PerObjectConstants constants;

    //Calculate MVP matrix for all primitives
    DirectX::XMVECTOR rotaxis = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationAxis(rotaxis, 0.0f);
    DirectX::XMMATRIX translation = DirectX::XMMatrixTranslation(0.0f, 0.0f, 0.0f);
    DirectX::XMMATRIX scale = DirectX::XMMatrixScaling(1.0f, 1.0f, 1.0f);
    constants.MVP = DirectX::XMMatrixIdentity();
    constants.MVP = translation * rotation * scale; //First calculate the model matrix
    //Then calculate model matrix with Model * View * Projection
    constants.MVP = DirectX::XMMatrixTranspose(constants.MVP * Cam.camView * Cam.camProjection);

    //Set MVP matrix in shader
    d3d11Context_->GSSetConstantBuffers(0, 1, perFrameBuffer_.GetAddressOf());
    perObjectBuffer_.SetData(d3d11Context_, &constants);

    //Draw linelist primitives
    d3d11Context_->Draw(numLineVertices_, 0);
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
        NeedsRedraw = true;
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

    //Setup rasterizer state used for meshes
    D3D11_RASTERIZER_DESC rasterizerDesc;
    ZeroMemory(&rasterizerDesc, sizeof(rasterizerDesc));
    rasterizerDesc.FillMode = D3D11_FILL_SOLID;
    rasterizerDesc.CullMode = D3D11_CULL_BACK;
    rasterizerDesc.FrontCounterClockwise = false;
    rasterizerDesc.DepthBias = false;
    rasterizerDesc.DepthBiasClamp = 0;
    rasterizerDesc.SlopeScaledDepthBias = 0;
    rasterizerDesc.DepthClipEnable = true;
    rasterizerDesc.ScissorEnable = false;
    rasterizerDesc.MultisampleEnable = false;
    rasterizerDesc.AntialiasedLineEnable = false;
    DxCheck(d3d11Device_->CreateRasterizerState(&rasterizerDesc, meshRasterizerState_.GetAddressOf()), "Mesh rasterizer state creation failed!");
}

void Scene::InitPrimitiveState()
{
    //Setup rasterizer state used for primitives
    D3D11_RASTERIZER_DESC rasterizerDesc;
    ZeroMemory(&rasterizerDesc, sizeof(rasterizerDesc));
    rasterizerDesc.FillMode = D3D11_FILL_SOLID;
    rasterizerDesc.CullMode = D3D11_CULL_NONE;
    rasterizerDesc.FrontCounterClockwise = false;
    rasterizerDesc.DepthBias = false;
    rasterizerDesc.DepthBiasClamp = 0;
    rasterizerDesc.SlopeScaledDepthBias = 0;
    rasterizerDesc.DepthClipEnable = true;
    rasterizerDesc.ScissorEnable = false;
    rasterizerDesc.MultisampleEnable = false;
    rasterizerDesc.AntialiasedLineEnable = false;
    DxCheck(d3d11Device_->CreateRasterizerState(&rasterizerDesc, primitiveRasterizerState_.GetAddressOf()), "Primitive rasterizer state creation failed!");

    //Create linelist primitive vertex buffer
    lineVertexBuffer_.Create(d3d11Device_, 1200, D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);

    linelistShader_.Load(linelistShaderPath_, d3d11Device_);

    //Create linelist vertex layout
    D3D11_INPUT_ELEMENT_DESC inputLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    auto pVSBlob = linelistShader_.GetVertexShaderBytes();
    //Create the input layout
    if (FAILED(d3d11Device_->CreateInputLayout(inputLayout, 2, pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), linelistVertexLayout_.GetAddressOf())))
        THROW_EXCEPTION("Failed to create scene primitive vertex layout");
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

void Scene::DrawLine(const Vec3& start, const Vec3& end, const Vec3& color)
{
    lineVertices_.push_back({ start, color });
    lineVertices_.push_back({ end, color });
    primitiveBufferNeedsUpdate_ = true;
}

void Scene::DrawBox(const Vec3& min, const Vec3& max, const Vec3& color)
{
    Vec3 diff = max - min;
    Vec3 vert0 = min;
    Vec3 vert1 = min + Vec3(diff.x, 0.0f, 0.0f);
    Vec3 vert2 = min + Vec3(0.0f, 0.0f, diff.z);
    Vec3 vert3 = min + Vec3(diff.x, 0.0f, diff.z);

    Vec3 vert4 = max;
    Vec3 vert5(vert1.x, max.y, vert1.z);
    Vec3 vert6(vert2.x, max.y, vert2.z);
    Vec3 vert7(min.x, max.y, min.z);

    DrawLine(vert0, vert1, color);
    DrawLine(vert0, vert2, color);
    DrawLine(vert0, vert7, color);
    DrawLine(vert1, vert3, color);
    DrawLine(vert1, vert5, color);
    DrawLine(vert2, vert3, color);
    DrawLine(vert2, vert6, color);
    DrawLine(vert3, vert4, color);
    DrawLine(vert4, vert5, color);
    DrawLine(vert4, vert6, color);
    DrawLine(vert5, vert7, color);
    DrawLine(vert6, vert7, color);

    primitiveBufferNeedsUpdate_ = true;
}

void Scene::ResetPrimitives()
{
    primitiveBufferNeedsUpdate_ = true;
    lineVertices_.clear();
}
