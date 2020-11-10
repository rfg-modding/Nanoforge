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
    InitTerrain();
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
    cbPerFrameObject.ViewPos = Cam.Position();
    cbPerFrameBuffer.SetData(d3d11Context_, &cbPerFrameObject);
    d3d11Context_->PSSetConstantBuffers(0, 1, cbPerFrameBuffer.GetAddressOf());

    //Draw terrain meshes
    d3d11Context_->IASetInputLayout(terrainVertexLayout_.Get());
    d3d11Context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    for (u32 i = 0; i < terrainInstanceRenderData_.size(); i++)
    {
        auto& renderInstance = terrainInstanceRenderData_[i];

        for (u32 j = 0; j < 9; j++)
        {
            shader_.Bind(d3d11Context_);

            DirectX::XMVECTOR rotaxis = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
            DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationAxis(rotaxis, 0.0f);
            DirectX::XMMATRIX translation = DirectX::XMMatrixTranslation(renderInstance.Position.x, renderInstance.Position.y, renderInstance.Position.z);

            terrainInstanceRenderData_[i].ModelMatrices[j] = DirectX::XMMatrixIdentity();
            terrainInstanceRenderData_[i].ModelMatrices[j] = translation * rotation;

            WVP = terrainInstanceRenderData_[i].ModelMatrices[j] * Cam.camView * Cam.camProjection;
            cbPerObjTerrain.WVP = XMMatrixTranspose(WVP);
            terrainPerObjectBuffer_.SetData(d3d11Context_, &cbPerObjTerrain);
            d3d11Context_->VSSetConstantBuffers(0, 1, terrainPerObjectBuffer_.GetAddressOf());

            renderInstance.Meshes[j].Bind(d3d11Context_);
            d3d11Context_->DrawIndexed(renderInstance.Meshes[j].NumIndices(), 0, 0);
        }
    }
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
    cbPerFrameBuffer.Create(d3d11Device_, sizeof(cbPerFrame), D3D11_BIND_CONSTANT_BUFFER);
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

void Scene::InitTerrain()
{
    LoadTerrainShaders();

    //Create buffer for per object constants
    terrainPerObjectBuffer_.Create(d3d11Device_, sizeof(cbPerObject), D3D11_BIND_CONSTANT_BUFFER);
}

void Scene::LoadTerrainShaders()
{
    shader_.Load(terrainShaderPath_, d3d11Device_);
    auto pVSBlob = shader_.GetVertexShaderBytes();

    //Create terrain vertex input layout
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0,  DXGI_FORMAT_R16G16B16A16_SINT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0,  DXGI_FORMAT_R32G32B32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    //Create the input layout
    if (FAILED(d3d11Device_->CreateInputLayout(layout, ARRAYSIZE(layout), pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), terrainVertexLayout_.GetAddressOf())))
        THROW_EXCEPTION("Failed to create terrain vertex layout");

    //Set the input layout
    d3d11Context_->IASetInputLayout(terrainVertexLayout_.Get());
}

void Scene::InitTerrainMeshes(std::vector<TerrainInstance>& terrainInstances)
{
    //Create terrain index & vertex buffers
    for (auto& instance : terrainInstances)
    {
        //Skip already-initialized terrain instances
        if (instance.RenderDataInitialized)
            continue;

        auto& renderInstance = terrainInstanceRenderData_.emplace_back();
        renderInstance.Position = instance.Position;

        for (u32 i = 0; i < 9; i++)
        {
            Mesh& mesh = renderInstance.Meshes[i];
            //Todo: Change this to take std::span<u8> as input so conversion isn't necessary
            mesh.Create(d3d11Device_, d3d11Context_, std::span<u8>((u8*)instance.Vertices[i].data(), instance.Vertices[i].size_bytes()), 
                                                     std::span<u8>((u8*)instance.Indices[i].data(), instance.Indices[i].size_bytes()), 
                                                     instance.Vertices[i].size(), DXGI_FORMAT_R16_UINT, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
            renderInstance.ModelMatrices[i] = DirectX::XMMATRIX();
#ifdef DEBUG_BUILD
            SetDebugName(mesh.GetIndexBuffer(), "terrain_index_buffer__" + instance.Name + std::to_string(i));
            SetDebugName(mesh.GetVertexBuffer(), "terrain_vertex_buffer__" + instance.Name + std::to_string(i));
#endif
        }

        //Set bool so the instance isn't initialized more than once
        instance.RenderDataInitialized = true;
    }
}