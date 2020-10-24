#include "Scene.h"
#include "render/util/DX11Helpers.h"
#include <filesystem>
#include "Log.h"

//Todo: Stick this in a debug namespace
void SetDebugName(ID3D11DeviceChild* child, const std::string& name)
{
    if (child != nullptr)
        child->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<u32>(name.size()), name.c_str());
}

//Todo: Toss this in a utility file somewhere
//Todo: Give better name like WidenCString
//Convert const char* to wchar_t*. Source: https://stackoverflow.com/a/8032108
const wchar_t* GetWC(const char* c)
{
    const size_t cSize = strlen(c) + 1;
    wchar_t* wc = new wchar_t[cSize];
    mbstowcs(wc, c, cSize);

    return wc;
}

void Scene::Init(ID3D11Device* d3d11Device, ID3D11DeviceContext* d3d11Context)
{
    d3d11Device_ = d3d11Device;
    d3d11Context_ = d3d11Context;

    InitInternal();
    InitRenderTarget();
    InitTerrain();

    terrainShaderWriteTime_ = std::filesystem::last_write_time(terrainShaderPath_);
}

void Scene::Cleanup()
{
    ReleaseCOM(terrainVertexShader_);
    ReleaseCOM(terrainPixelShader_);
    ReleaseCOM(terrainVertexLayout_);
    for (auto& instance : terrainInstanceRenderData_)
    {
        //Todo: Figure out why final buffer pair have a reference count of 0. It's likely leaking memory here by not clearing the last buffer.
        //Note: We don't release the last pair of buffers as doing so causes a crash due to them having a ref count of 0. Unsure why the refcount is zero
        for (u32 i = 0; i < instance.terrainVertexBuffers_.size() - 1; i++)
        {
            auto* vertexBuffer = instance.terrainVertexBuffers_[i];
            auto* indexBuffer = instance.terrainIndexBuffers_[i];
            ReleaseCOM(vertexBuffer);
            ReleaseCOM(indexBuffer);
        }
    }
    ReleaseCOM(terrainPerObjectBuffer_);

    ReleaseCOM(cbPerFrameBuffer);
    ReleaseCOM(cbPerObjectBuffer);

    ReleaseCOM(depthBuffer_);
    ReleaseCOM(depthBufferView_);
}

void Scene::Draw()
{
    //Reload shaders if necessary
    const auto latestShaderWriteTime = std::filesystem::last_write_time(terrainShaderPath_);
    if (latestShaderWriteTime != terrainShaderWriteTime_)
    {
        terrainShaderWriteTime_ = latestShaderWriteTime;
        Log->info("Reloading shaders...");
        //Wait for a moment as a quickfix to a crash that happens when we read the shader as it's being saved
        Sleep(250);
        LoadTerrainShaders(true);
        Log->info("Shaders reloaded.");
    }

    //Set render target and clear it
    d3d11Context_->ClearRenderTargetView(sceneViewRenderTarget_, reinterpret_cast<float*>(&clearColor));
    d3d11Context_->ClearDepthStencilView(depthBufferView_, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    d3d11Context_->OMSetRenderTargets(1, &sceneViewRenderTarget_, depthBufferView_);
    d3d11Context_->RSSetViewports(1, &sceneViewport_);

    //Update per-frame constant buffer
    cbPerFrameObject.ViewPos = Cam.Position();
    d3d11Context_->UpdateSubresource(cbPerFrameBuffer, 0, NULL, &cbPerFrameObject, 0, 0);
    d3d11Context_->PSSetConstantBuffers(0, 1, &cbPerFrameBuffer);

    //Draw terrain meshes
    d3d11Context_->IASetInputLayout(terrainVertexLayout_);
    d3d11Context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    for (u32 i = 0; i < terrainInstanceRenderData_.size(); i++)
    {
        auto& renderInstance = terrainInstanceRenderData_[i];

        for (u32 j = 0; j < 9; j++)
        {
            d3d11Context_->VSSetShader(terrainVertexShader_, nullptr, 0);
            d3d11Context_->PSSetShader(terrainPixelShader_, nullptr, 0);

            DirectX::XMVECTOR rotaxis = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
            DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationAxis(rotaxis, 0.0f);
            DirectX::XMMATRIX translation = DirectX::XMMatrixTranslation(renderInstance.Position.x, renderInstance.Position.y, renderInstance.Position.z);

            terrainModelMatrices_[i] = DirectX::XMMatrixIdentity();
            terrainModelMatrices_[i] = translation * rotation;

            WVP = terrainModelMatrices_[i] * Cam.camView * Cam.camProjection;
            cbPerObjTerrain.WVP = XMMatrixTranspose(WVP);
            d3d11Context_->UpdateSubresource(terrainPerObjectBuffer_, 0, NULL, &cbPerObjTerrain, 0, 0);
            d3d11Context_->VSSetConstantBuffers(0, 1, &terrainPerObjectBuffer_);

            d3d11Context_->IASetIndexBuffer(renderInstance.terrainIndexBuffers_[j], DXGI_FORMAT_R16_UINT, 0);
            d3d11Context_->IASetVertexBuffers(0, 1, &renderInstance.terrainVertexBuffers_[j], &terrainVertexStride, &terrainVertexOffset);

            d3d11Context_->DrawIndexed(renderInstance.MeshIndexCounts_[j], 0, 0);
        }
    }
}

void Scene::HandleResize(int windowWidth, int windowHeight)
{
    if ((u32)windowWidth != sceneViewWidth_ || (u32)windowHeight != sceneViewHeight_)
    {
        sceneViewWidth_ = (u32)windowWidth;
        sceneViewHeight_ = (u32)windowHeight;
        
        //Recreate scene view resources with new size
        InitInternal();
        CreateDepthBuffer(false);

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
    //Release scene texture and view if they've already been initialized
    if (sceneViewShaderResource_)
        ReleaseCOM(sceneViewShaderResource_);
    if (sceneViewRenderTarget_)
        ReleaseCOM(sceneViewRenderTarget_);
    if (sceneViewTexture_)
        ReleaseCOM(sceneViewTexture_);
    if (cbPerFrameBuffer)
        ReleaseCOM(cbPerFrameBuffer);

    //Create texture the view is rendered to
    D3D11_TEXTURE2D_DESC textureDesc;
    ZeroMemory(&textureDesc, sizeof(D3D11_TEXTURE2D_DESC));
    textureDesc.Width = sceneViewWidth_;
    textureDesc.Height = sceneViewHeight_;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    textureDesc.CPUAccessFlags = 0;
    textureDesc.MiscFlags = 0;
    d3d11Device_->CreateTexture2D(&textureDesc, nullptr, &sceneViewTexture_);
    if (!sceneViewTexture_)
        THROW_EXCEPTION("sceneViewTexture_ nullptr after call to CreateTexture2D in DX11Renderer::InitScene()");

    //Create the render target view for the scene
    D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
    renderTargetViewDesc.Format = textureDesc.Format;
    renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    renderTargetViewDesc.Texture2D.MipSlice = 0;

    //Create the scene render target view and map it to the scene texture
    d3d11Device_->CreateRenderTargetView(sceneViewTexture_, &renderTargetViewDesc, &sceneViewRenderTarget_);

    //Create shader resource for dear imgui to use
    D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;
    shaderResourceViewDesc.Format = textureDesc.Format;
    shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
    shaderResourceViewDesc.Texture2D.MipLevels = 1;

    //Todo: Add error code and (if possible) error string reporting to the DxCheck macro
    //Create the shader resource view.
    HRESULT result = d3d11Device_->CreateShaderResourceView(sceneViewTexture_, &shaderResourceViewDesc, &sceneViewShaderResource_);

#ifdef DEBUG_BUILD
    SetDebugName(sceneViewTexture_, "sceneViewTexture_");
    SetDebugName(sceneViewRenderTarget_, "sceneViewRenderTarget_");
    SetDebugName(sceneViewShaderResource_, "sceneViewShaderResource_");
#endif

    //Create per-frame constant buffer
    D3D11_BUFFER_DESC cbbd;
    ZeroMemory(&cbbd, sizeof(D3D11_BUFFER_DESC));
    cbbd.Usage = D3D11_USAGE_DEFAULT;
    cbbd.ByteWidth = sizeof(cbPerFrame);
    cbbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbbd.CPUAccessFlags = 0;
    cbbd.MiscFlags = 0;
    result = d3d11Device_->CreateBuffer(&cbbd, NULL, &cbPerFrameBuffer);
}

void Scene::InitRenderTarget()
{
    CreateDepthBuffer();
    d3d11Context_->OMSetRenderTargets(1, &sceneViewRenderTarget_, depthBufferView_);
    d3d11Context_->RSSetViewports(1, &sceneViewport_);
}

void Scene::CreateDepthBuffer(bool firstResize)
{
    //Clear depth resources if we're re-initing them
    if (!firstResize && depthBuffer_)
        ReleaseCOM(depthBuffer_);
    if (!firstResize && depthBufferView_)
        ReleaseCOM(depthBufferView_);

    D3D11_TEXTURE2D_DESC depthBufferDesc;
    depthBufferDesc.Width = sceneViewWidth_;
    depthBufferDesc.Height = sceneViewHeight_;
    depthBufferDesc.MipLevels = 1;
    depthBufferDesc.ArraySize = 1;
    depthBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    depthBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    depthBufferDesc.CPUAccessFlags = 0;
    depthBufferDesc.MiscFlags = 0;
    depthBufferDesc.SampleDesc.Count = 1;
    depthBufferDesc.SampleDesc.Quality = 0;

    if (FAILED(d3d11Device_->CreateTexture2D(&depthBufferDesc, 0, &depthBuffer_)))
        THROW_EXCEPTION("Failed to create depth buffer texture");
    if (FAILED(d3d11Device_->CreateDepthStencilView(depthBuffer_, 0, &depthBufferView_)))
        THROW_EXCEPTION("Failed to create depth buffer view");
}

void Scene::InitTerrain()
{
    LoadTerrainShaders(false);

    //Create buffer for MVP matrix constant in shader
    D3D11_BUFFER_DESC cbbd;
    ZeroMemory(&cbbd, sizeof(D3D11_BUFFER_DESC));

    //Create per object buffer
    cbbd.Usage = D3D11_USAGE_DEFAULT;
    cbbd.ByteWidth = sizeof(cbPerObject);
    cbbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbbd.CPUAccessFlags = 0;
    cbbd.MiscFlags = 0;

    HRESULT result = d3d11Device_->CreateBuffer(&cbbd, nullptr, &terrainPerObjectBuffer_);
    if (FAILED(result))
        THROW_EXCEPTION("Failed to create terrain uniform buffer.");
}

void Scene::LoadTerrainShaders(bool reload)
{
    if (reload)
    {
        ReleaseCOM(terrainVertexShader_);
        ReleaseCOM(terrainPixelShader_);
        ReleaseCOM(terrainVertexLayout_);
    }

    //Compile & create terrain vertex and pixel shaders
    ID3DBlob* pVSBlob = nullptr;
    ID3DBlob* pPSBlob = nullptr;

    //Compile the vertex shader
    if (FAILED(CompileShaderFromFile(GetWC(terrainShaderPath_.c_str()), "VS", "vs_4_0", &pVSBlob)))
    {
        THROW_EXCEPTION("Failed to compile terrain vertex shader!");
    }
    if (FAILED(d3d11Device_->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &terrainVertexShader_)))
    {
        pVSBlob->Release();
        THROW_EXCEPTION("Failed to create terrain vertex shader!");
    }
    //Compile the pixel shader
    if (FAILED(CompileShaderFromFile(GetWC(terrainShaderPath_.c_str()), "PS", "ps_4_0", &pPSBlob)))
    {
        THROW_EXCEPTION("Failed to compile terrain pixel shader!");
    }
    if (FAILED(d3d11Device_->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &terrainPixelShader_)))
    {
        pPSBlob->Release();
        THROW_EXCEPTION("Failed to create terrain pixel shader!");
    }

    //Create terrain vertex input layout
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0,  DXGI_FORMAT_R16G16B16A16_SINT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0,  DXGI_FORMAT_R32G32B32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    //Create the input layout
    if (FAILED(d3d11Device_->CreateInputLayout(layout, ARRAYSIZE(layout), pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &terrainVertexLayout_)))
        THROW_EXCEPTION("Failed to create terrain vertex layout");

    //Set the input layout
    d3d11Context_->IASetInputLayout(terrainVertexLayout_);

    ReleaseCOM(pVSBlob);
    ReleaseCOM(pPSBlob);
}

void Scene::ResetTerritoryData()
{
    for (auto& instance : terrainInstanceRenderData_)
    {
        //Todo: Figure out why final buffer pair have a reference count of 0. It's likely leaking memory here by not clearing the last buffer.
        //Note: We don't release the last pair of buffers as doing so causes a crash due to them having a ref count of 0. Unsure why the refcount is zero
        for (u32 i = 0; i < instance.terrainVertexBuffers_.size() - 1; i++)
        {
            auto* vertexBuffer = instance.terrainVertexBuffers_[i];
            auto* indexBuffer = instance.terrainIndexBuffers_[i];
            ReleaseCOM(vertexBuffer);
            ReleaseCOM(indexBuffer);
        }
    }
    terrainInstanceRenderData_.clear();
    terrainModelMatrices_.clear();
}

void Scene::InitTerrainMeshes(std::vector<TerrainInstance>* terrainInstances)
{
    //Create terrain index & vertex buffers
    for (auto& instance : *terrainInstances)
    {
        //Skip already-initialized terrain instances
        if (instance.RenderDataInitialized)
            continue;

        auto& renderInstance = terrainInstanceRenderData_.emplace_back();
        renderInstance.Position = instance.Position;

        for (u32 i = 0; i < 9; i++)
        {
            ID3D11Buffer* terrainIndexBuffer = nullptr;
            ID3D11Buffer* terrainVertexBuffer = nullptr;

            //Create index buffer for instance
            D3D11_BUFFER_DESC indexBufferDesc = {};
            indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
            indexBufferDesc.ByteWidth = static_cast<u32>(instance.Indices[i].size_bytes());
            indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
            indexBufferDesc.CPUAccessFlags = 0;
            indexBufferDesc.MiscFlags = 0;

            D3D11_SUBRESOURCE_DATA indexBufferInitData;
            indexBufferInitData.pSysMem = instance.Indices[i].data();
            HRESULT hr = d3d11Device_->CreateBuffer(&indexBufferDesc, &indexBufferInitData, &terrainIndexBuffer);
            if (FAILED(hr))
                THROW_EXCEPTION("Failed to create a terrain index buffer!");

            d3d11Context_->IASetIndexBuffer(terrainIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

            //Create vertex buffer for instance
            D3D11_BUFFER_DESC vertexBufferDesc = {};
            vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
            vertexBufferDesc.ByteWidth = static_cast<u32>(instance.Vertices[i].size_bytes());
            vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            vertexBufferDesc.CPUAccessFlags = 0;

            D3D11_SUBRESOURCE_DATA vertexBufferInitData = {};
            vertexBufferInitData.pSysMem = instance.Vertices[i].data();
            hr = d3d11Device_->CreateBuffer(&vertexBufferDesc, &vertexBufferInitData, &terrainVertexBuffer);
            if (FAILED(hr))
                THROW_EXCEPTION("Failed to create a terrain vertex buffer!");

            //Set vertex buffer
            terrainVertexStride = sizeof(LowLodTerrainVertex);
            terrainVertexOffset = 0;
            d3d11Context_->IASetVertexBuffers(0, 1, &terrainVertexBuffer, &terrainVertexStride, &terrainVertexOffset);

            //Set primitive topology
            d3d11Context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

#ifdef DEBUG_BUILD
            SetDebugName(terrainIndexBuffer, "terrain_index_buffer__" + instance.Name + std::to_string(i));
            SetDebugName(terrainVertexBuffer, "terrain_vertex_buffer__" + instance.Name + std::to_string(i));
#endif

            renderInstance.terrainIndexBuffers_[i] = terrainIndexBuffer;
            renderInstance.terrainVertexBuffers_[i] = terrainVertexBuffer;
            renderInstance.MeshIndexCounts_[i] = static_cast<u32>(instance.Indices[i].size());
            terrainModelMatrices_.push_back(DirectX::XMMATRIX());
        }

        //Set bool so the instance isn't initialized more than once
        instance.RenderDataInitialized = true;
    }
}