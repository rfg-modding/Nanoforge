#include "Scene.h"
#include "render/util/DX11Helpers.h"
#include <filesystem>
#include "Log.h"
#include "util/StringHelpers.h"
#include "application/Config.h"
#include "render/Render.h"
#include "util/Profiler.h"

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

void Scene::Draw(f32 deltaTime)
{
    if (errorOccurred_ || !linelistMaterial_ || !linelistMaterial_->Ready() || !trianglelistMaterial_ || !trianglelistMaterial_->Ready())
        return;

    PROFILER_FUNCTION();
    TotalTime += deltaTime;

    //Reload shaders if necessary
    Render::ReloadEditedShaders();

    //Set render target and clear it
    d3d11Context_->ClearRenderTargetView(sceneViewTexture_.GetRenderTargetView(), reinterpret_cast<const float*>(&ClearColor));
    d3d11Context_->ClearDepthStencilView(depthBufferTexture_.GetDepthStencilView(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    d3d11Context_->OMSetRenderTargets(1, sceneViewTexture_.GetRenderTargetViewPP(), depthBufferTexture_.GetDepthStencilView());
    d3d11Context_->RSSetViewports(1, &sceneViewport_);

    //Prepare state to render triangle strip meshes
    d3d11Context_->RSSetState(meshRasterizerState_.Get());
    d3d11Context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    //Update per-frame constant buffer
    perFrameStagingBuffer_.Time += deltaTime;
    perFrameStagingBuffer_.ViewPos = Cam.Position();
    perFrameStagingBuffer_.ViewportDimensions = Vec2{ (f32)sceneViewWidth_, (f32)sceneViewHeight_ };
    perFrameBuffer_.SetData(d3d11Context_, &perFrameStagingBuffer_);
    d3d11Context_->PSSetConstantBuffers(0, 1, perFrameBuffer_.GetAddressOf());

    //Update per object buffer
    d3d11Context_->VSSetConstantBuffers(0, 1, perObjectBuffer_.GetAddressOf());

    //Draw render objects
    {
        std::lock_guard<std::mutex> lock(ObjectCreationMutex);

        //Batch render objects by material
        for (auto& kv : objectMaterials_)
        {
            //Get and bind the material
            Material* material = Render::GetMaterial(kv.first);
            if(!material)
            {
                LOG_ERROR("Failed to get material '{}'", kv.first);
                continue;
            }
            material->Use(d3d11Context_);

            //Render all objects using the material
            for (Handle<RenderObject> renderObject : kv.second)
                renderObject->Draw(d3d11Context_, perObjectBuffer_, Cam);
        }
    }

    //Prepare state to render primitives
    d3d11Context_->RSSetState(primitiveRasterizerState_.Get());

    //Update primitive vertex buffers if necessary
    if (primitiveBufferNeedsUpdate_)
    {
        //Update line list vertex buffer
        {
            lineVertexBuffer_.ResizeIfNeeded(d3d11Device_, (u32)lineVertices_.size() * sizeof(ColoredVertex));
            D3D11_MAPPED_SUBRESOURCE mappedResource;
            ZeroMemory(&mappedResource, sizeof(D3D11_MAPPED_SUBRESOURCE));
            d3d11Context_->Map(lineVertexBuffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
            memcpy(mappedResource.pData, lineVertices_.data(), lineVertices_.size() * sizeof(ColoredVertex));
            d3d11Context_->Unmap(lineVertexBuffer_.Get(), 0);
            numLineVertices_ = (u32)lineVertices_.size();
            lineVertices_.clear();
        }

        //Update triangle list vertex buffer
        {
            triangleListVertexBuffer_.ResizeIfNeeded(d3d11Device_, (u32)triangleListVertices_.size() * sizeof(ColoredVertex));
            D3D11_MAPPED_SUBRESOURCE mappedResource;
            ZeroMemory(&mappedResource, sizeof(D3D11_MAPPED_SUBRESOURCE));
            d3d11Context_->Map(triangleListVertexBuffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
            memcpy(mappedResource.pData, triangleListVertices_.data(), triangleListVertices_.size() * sizeof(ColoredVertex));
            d3d11Context_->Unmap(triangleListVertexBuffer_.Get(), 0);
            numTriangleListVertices_ = (u32)triangleListVertices_.size();
            triangleListVertices_.clear();
        }

        //Update lit triangle list vertex buffer
        {
            litTriangleListVertexBuffer_.ResizeIfNeeded(d3d11Device_, (u32)litTriangleListVertices_.size() * sizeof(ColoredVertexLit));
            D3D11_MAPPED_SUBRESOURCE mappedResource;
            ZeroMemory(&mappedResource, sizeof(D3D11_MAPPED_SUBRESOURCE));
            d3d11Context_->Map(litTriangleListVertexBuffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
            memcpy(mappedResource.pData, litTriangleListVertices_.data(), litTriangleListVertices_.size() * sizeof(ColoredVertexLit));
            d3d11Context_->Unmap(litTriangleListVertexBuffer_.Get(), 0);
            numLitTriangleListVertices_ = (u32)litTriangleListVertices_.size();
            litTriangleListVertices_.clear();
        }

        primitiveBufferNeedsUpdate_ = false;
    }

    //Update primitive shader constants buffer
    {
        PerObjectConstants constants;

        //Calculate MVP matrix for all primitives
        DirectX::XMVECTOR rotaxis = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationAxis(rotaxis, 0.0f);
        DirectX::XMMATRIX translation = DirectX::XMMatrixTranslation(0.0f, 0.0f, 0.0f);
        DirectX::XMMATRIX scale = DirectX::XMMatrixScaling(1.0f, 1.0f, 1.0f);
        constants.MVP = DirectX::XMMatrixIdentity();
        constants.MVP = translation * rotation * scale; //First calculate the model matrix
        constants.MVP = DirectX::XMMatrixTranspose(constants.MVP * Cam.camView * Cam.camProjection); //Then calculate MVP matrix with Model * View * Projection

        d3d11Context_->GSSetConstantBuffers(0, 1, perFrameBuffer_.GetAddressOf());
        perObjectBuffer_.SetData(d3d11Context_, &constants);
    }
    u32 offset = 0; //Dummy variable used by IASetVertBuffers. Passing nullptr doesn't work even though the MSDN docs say the arg is optional

    //Draw linelist primitives
    u32 strideColoredVertex = sizeof(ColoredVertex);
    linelistMaterial_->Use(d3d11Context_);
    d3d11Context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    d3d11Context_->IASetVertexBuffers(0, 1, lineVertexBuffer_.GetAddressOf(), &strideColoredVertex, &offset);
    d3d11Context_->Draw(numLineVertices_, 0);

    //Draw unlit triangle list primitives
    d3d11Context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    offset = 0;
    trianglelistMaterial_->Use(d3d11Context_);
    d3d11Context_->IASetVertexBuffers(0, 1, triangleListVertexBuffer_.GetAddressOf(), &strideColoredVertex, &offset);
    d3d11Context_->Draw(numTriangleListVertices_, 0);

    //Draw lit triangle list primitives
    offset = 0;
    u32 strideLit = sizeof(ColoredVertexLit);
    litTrianglelistMaterial_->Use(d3d11Context_);
    d3d11Context_->IASetVertexBuffers(0, 1, litTriangleListVertexBuffer_.GetAddressOf(), &strideLit, &offset);
    d3d11Context_->Draw(numLitTriangleListVertices_, 0);
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

    //Create primitive vertex buffers
    lineVertexBuffer_.Create(d3d11Device_, sizeof(ColoredVertex) * 100, D3D11_BIND_VERTEX_BUFFER, nullptr, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
    triangleListVertexBuffer_.Create(d3d11Device_, sizeof(ColoredVertex) * 100, D3D11_BIND_VERTEX_BUFFER, nullptr, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
    litTriangleListVertexBuffer_.Create(d3d11Device_, sizeof(ColoredVertexLit) * 100, D3D11_BIND_VERTEX_BUFFER, nullptr, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);

    linelistMaterial_ = Render::GetMaterial("Linelist");
    if (!linelistMaterial_)
    {
        LOG_ERROR("Failed to locate material 'Linelist' for Scene primitive rendering. Scene disabled.");
        errorOccurred_ = true;
        return;
    }

    trianglelistMaterial_ = Render::GetMaterial("SolidTriList");
    if (!trianglelistMaterial_)
    {
        LOG_ERROR("Failed to locate material 'SolidTriList' for Scene primitive rendering. Scene disabled.");
        errorOccurred_ = true;
        return;
    }

    litTrianglelistMaterial_ = Render::GetMaterial("LitTriList");
    if (!litTrianglelistMaterial_)
    {
        LOG_ERROR("Failed to locate material 'LitTriList' for Scene primitive rendering. Scene disabled.");
        errorOccurred_ = true;
        return;
    }
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
    lineVertices_.push_back({ start, (u8)(color.x * 255), (u8)(color.y * 255), (u8)(color.z * 255) });
    lineVertices_.push_back({ end, (u8)(color.x * 255), (u8)(color.y * 255), (u8)(color.z * 255) });
    primitiveBufferNeedsUpdate_ = true;
}

void Scene::DrawQuad(const Vec3& bottomLeft, const Vec3& topLeft, const Vec3& topRight, const Vec3& bottomRight, const Vec3& color)
{
    DrawLine(bottomLeft, topLeft, color);
    DrawLine(topLeft, topRight, color);
    DrawLine(topRight, bottomRight, color);
    DrawLine(bottomRight, bottomLeft, color);
}

void Scene::DrawBox(const Vec3& min, const Vec3& max, const Vec3& color)
{
    Vec3 size = max - min;
    Vec3 bottomLeftFront = min;
    Vec3 bottomLeftBack = min + Vec3(size.x, 0.0f, 0.0f);
    Vec3 bottomRightFront = min + Vec3(0.0f, 0.0f, size.z);
    Vec3 bottomRightBack = min + Vec3(size.x, 0.0f, size.z);

    Vec3 topRightBack = max;
    Vec3 topLeftBack(bottomLeftBack.x, max.y, bottomLeftBack.z);
    Vec3 topRightFront(bottomRightFront.x, max.y, bottomRightFront.z);
    Vec3 topLeftFront(min.x, max.y, min.z);

    //Draw quads for the front and back faces
    DrawQuad(bottomLeftFront, topLeftFront, topRightFront, bottomRightFront, color);
    DrawQuad(bottomLeftBack, topLeftBack, topRightBack, bottomRightBack, color);

    //Draw lines connecting the two faces
    DrawLine(bottomLeftFront, bottomLeftBack, color);
    DrawLine(topLeftFront, topLeftBack, color);
    DrawLine(topRightFront, topRightBack, color);
    DrawLine(bottomRightFront, bottomRightBack, color);

    primitiveBufferNeedsUpdate_ = true;
}

void Scene::DrawQuadSolid(const Vec3& bottomLeft, const Vec3& topLeft, const Vec3& topRight, const Vec3& bottomRight, const Vec3& color)
{
    //First triangle
    triangleListVertices_.push_back({ bottomLeft, (u8)(color.x * 255), (u8)(color.y * 255), (u8)(color.z * 255) });
    triangleListVertices_.push_back({ topLeft, (u8)(color.x * 255), (u8)(color.y * 255), (u8)(color.z * 255) });
    triangleListVertices_.push_back({ topRight, (u8)(color.x * 255), (u8)(color.y * 255), (u8)(color.z * 255) });

    //Second triangle
    triangleListVertices_.push_back({ topRight, (u8)(color.x * 255), (u8)(color.y * 255), (u8)(color.z * 255) });
    triangleListVertices_.push_back({ bottomRight, (u8)(color.x * 255), (u8)(color.y * 255), (u8)(color.z * 255) });
    triangleListVertices_.push_back({ bottomLeft, (u8)(color.x * 255), (u8)(color.y * 255), (u8)(color.z * 255) });

    primitiveBufferNeedsUpdate_ = true;
}

void Scene::DrawBoxSolid(const Vec3& min, const Vec3& max, const Vec3& color)
{
    Vec3 size = max - min;
    Vec3 bottomLeftFront = min;
    Vec3 bottomLeftBack = min + Vec3(size.x, 0.0f, 0.0f);
    Vec3 bottomRightFront = min + Vec3(0.0f, 0.0f, size.z);
    Vec3 bottomRightBack = min + Vec3(size.x, 0.0f, size.z);

    Vec3 topRightBack = max;
    Vec3 topLeftBack(bottomLeftBack.x, max.y, bottomLeftBack.z);
    Vec3 topRightFront(bottomRightFront.x, max.y, bottomRightFront.z);
    Vec3 topLeftFront(min.x, max.y, min.z);

    //Draw quads for each face
    DrawQuadSolid(bottomLeftFront, topLeftFront, topRightFront, bottomRightFront, color);     //Front
    DrawQuadSolid(bottomLeftBack, topLeftBack, topRightBack, bottomRightBack, color);         //Back
    DrawQuadSolid(bottomLeftBack, topLeftBack, topLeftFront, bottomLeftFront, color);         //Left
    DrawQuadSolid(bottomRightFront, topRightFront, topRightBack, bottomRightBack, color);     //Right
    DrawQuadSolid(topLeftFront, topLeftBack, topRightBack, topRightFront, color);             //Top
    DrawQuadSolid(bottomLeftFront, bottomLeftBack, bottomRightBack, bottomRightFront, color); //Bottom

    primitiveBufferNeedsUpdate_ = true;
}

void Scene::DrawQuadLit(const Vec3& bottomLeft, const Vec3& topLeft, const Vec3& topRight, const Vec3& bottomRight, const Vec3& normal, const Vec3& color)
{
    //First triangle
    litTriangleListVertices_.push_back({ bottomLeft, normal, (u8)(color.x * 255), (u8)(color.y * 255), (u8)(color.z * 255) });
    litTriangleListVertices_.push_back({ topLeft, normal, (u8)(color.x * 255), (u8)(color.y * 255), (u8)(color.z * 255) });
    litTriangleListVertices_.push_back({ topRight, normal, (u8)(color.x * 255), (u8)(color.y * 255), (u8)(color.z * 255) });

    //Second triangle
    litTriangleListVertices_.push_back({ topRight, normal, (u8)(color.x * 255), (u8)(color.y * 255), (u8)(color.z * 255) });
    litTriangleListVertices_.push_back({ bottomRight, normal, (u8)(color.x * 255), (u8)(color.y * 255), (u8)(color.z * 255) });
    litTriangleListVertices_.push_back({ bottomLeft, normal, (u8)(color.x * 255), (u8)(color.y * 255), (u8)(color.z * 255) });

    primitiveBufferNeedsUpdate_ = true;
}

void Scene::DrawBoxLit(const Vec3& min, const Vec3& max, const Vec3& color)
{
    Vec3 size = max - min;
    Vec3 bottomLeftFront = min;
    Vec3 bottomLeftBack = min + Vec3(size.x, 0.0f, 0.0f);
    Vec3 bottomRightFront = min + Vec3(0.0f, 0.0f, size.z);
    Vec3 bottomRightBack = min + Vec3(size.x, 0.0f, size.z);

    Vec3 topRightBack = max;
    Vec3 topLeftBack(bottomLeftBack.x, max.y, bottomLeftBack.z);
    Vec3 topRightFront(bottomRightFront.x, max.y, bottomRightFront.z);
    Vec3 topLeftFront(min.x, max.y, min.z);

    //Draw quads for each face
    DrawQuadLit(bottomLeftFront, topLeftFront, topRightFront, bottomRightFront, { -1.0f, 0.0f, 0.0f }, color);     //Front
    DrawQuadLit(bottomLeftBack, topLeftBack, topRightBack, bottomRightBack, { 1.0f, 0.0f, 0.0f }, color);          //Back
    DrawQuadLit(bottomLeftBack, topLeftBack, topLeftFront, bottomLeftFront, { 0.0f, 0.0f, -1.0f }, color);         //Left
    DrawQuadLit(bottomRightFront, topRightFront, topRightBack, bottomRightBack, { 0.0f, 0.0f, 1.0f }, color);      //Right
    DrawQuadLit(topLeftFront, topLeftBack, topRightBack, topRightFront, { 0.0f, 1.0f, 0.0f }, color);              //Top
    DrawQuadLit(bottomLeftFront, bottomLeftBack, bottomRightBack, bottomRightFront, { 0.0f, -1.0f, 0.0f }, color); //Bottom

    primitiveBufferNeedsUpdate_ = true;
}

void Scene::ResetPrimitives()
{
    primitiveBufferNeedsUpdate_ = true;
    lineVertices_.clear();
    triangleListVertices_.clear();
    litTriangleListVertices_.clear();
}

Handle<RenderObject> Scene::CreateRenderObject(std::string_view materialName, const Mesh& mesh, const Vec3& position)
{
    std::lock_guard<std::mutex> lock(ObjectCreationMutex);
    Handle<RenderObject> obj = CreateHandle<RenderObject>(mesh, position);
    Objects.push_back(obj);

    //Map object to its material
    auto materialObjectList = objectMaterials_.try_emplace(string(materialName), std::vector<Handle<RenderObject>>());
    materialObjectList.first->second.push_back(obj); //Add obj to list of objects that use the same material

    return obj;
}
