#include "DX11Renderer.h"
#include "render/imgui/ImGuiFontManager.h"
#include "render/camera/Camera.h"
#include <ext/WindowsWrapper.h>
#include <exception>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")
#include <dxgi.h>
#include <d3dcommon.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <directxcolors.h>
#include <imgui/imgui.h>
#include <imgui/imconfig.h>
#include <imgui/examples/imgui_impl_win32.h>
#include <imgui/examples/imgui_impl_dx11.h>
#include <DirectXTex.h>
#include <Dependencies\DirectXTex\DirectXTex\DirectXTexD3D11.cpp>
#include "render/util/DX11Helpers.h"
#include "render/backend/Im3dRenderer.h"
#include <iostream>
#include "Log.h"

//Todo: Stick this in a debug namespace
void SetDebugName(ID3D11DeviceChild* child, const std::string& name)
{
    if (child != nullptr)
        child->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)name.size(), name.c_str());
}

void DX11Renderer::Init(HINSTANCE hInstance, WNDPROC wndProc, int WindowWidth, int WindowHeight, ImGuiFontManager* fontManager, Camera* camera)
{
    hInstance_ = hInstance;
    windowWidth_ = WindowWidth;
    windowHeight_ = WindowHeight;
    fontManager_ = fontManager;
    camera_ = camera;
    im3dRenderer_ = new Im3dRenderer;

    if (!InitWindow(wndProc))
        THROW_EXCEPTION("Failed to init window! Exiting.");

    UpdateWindowDimensions();
    if (!InitDx11())
        THROW_EXCEPTION("Failed to init DX11! Exiting.");
    if (!InitImGui())
        THROW_EXCEPTION("Failed to dear imgui! Exiting.");
    if (!InitScene())
        THROW_EXCEPTION("Failed to init render scene! Exiting.");

    im3dRenderer_->Init(d3d11Device_, d3d11Context_, hwnd_, camera_);
    terrainShaderWriteTime_ = std::filesystem::last_write_time(terrainShaderPath_);

    //Needed by some DirectXTex functions
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    //Quick fix for view being distorted before first window resize
    HandleResize();

    //Init terrain shaders and buffers that don't need to be streamed in
    InitTerrainResources();

    initialized_ = true;
}

DX11Renderer::~DX11Renderer()
{
    if (!initialized_)
        return;

    if (im3dRenderer_)
    {
        im3dRenderer_->Shutdown();
        delete im3dRenderer_;
    }

    //Shutdown dear imgui
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    //Release DX11 resources
    ReleaseCOM(terrainVertexShader_);
    ReleaseCOM(terrainPixelShader_);
    ReleaseCOM(terrainVertexLayout_);
    //for (auto& instance : terrainInstanceRenderData_)
    //    for(auto& vertexBuffer : instance.terrainVertexBuffers_)
    //        ReleaseCOM(vertexBuffer);
    //for (auto& instance : terrainInstanceRenderData_)
    //    for (auto& indexBuffer : instance.terrainVertexBuffers_)
    //        ReleaseCOM(indexBuffer);

    ReleaseCOM(terrainPerObjectBuffer_);
    
    ReleaseCOM(cbPerFrameBuffer);
    ReleaseCOM(depthBuffer_);
    ReleaseCOM(depthBufferView_);
    ReleaseCOM(SwapChain_);
    ReleaseCOM(d3d11Device_);
    ReleaseCOM(d3d11Context_);
    ReleaseCOM(dxgiFactory_);
    ReleaseCOM(vertexShader_);
    ReleaseCOM(pixelShader_);
    ReleaseCOM(vertexLayout_);
    ReleaseCOM(vertexBuffer_);
    ReleaseCOM(indexBuffer_);
    ReleaseCOM(cbPerObjectBuffer);
}

void DX11Renderer::NewFrame(f32 deltaTime)
{
    im3dRenderer_->NewFrame(deltaTime);
}

void DX11Renderer::DoFrame(f32 deltaTime)
{
    //Reload shaders if necessary
    auto latestShaderWriteTime = std::filesystem::last_write_time(terrainShaderPath_);
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
    d3d11Context_->OMSetBlendState(blendState_, nullptr, 0xffffffff);
    d3d11Context_->OMSetDepthStencilState(depthStencilState_, 0);
    d3d11Context_->RSSetState(rasterizerState_);

    //Update per-frame constant buffer
    cbPerFrameObject.ViewPos = camera_->Position();
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

            WVP = terrainModelMatrices_[i] * camera_->camView * camera_->camProjection;
            cbPerObjTerrain.WVP = XMMatrixTranspose(WVP);
            d3d11Context_->UpdateSubresource(terrainPerObjectBuffer_, 0, NULL, &cbPerObjTerrain, 0, 0);
            d3d11Context_->VSSetConstantBuffers(0, 1, &terrainPerObjectBuffer_);

            d3d11Context_->IASetIndexBuffer(renderInstance.terrainIndexBuffers_[j], DXGI_FORMAT_R16_UINT, 0);
            d3d11Context_->IASetVertexBuffers(0, 1, &renderInstance.terrainVertexBuffers_[j], &terrainVertexStride, &terrainVertexOffset);

            d3d11Context_->DrawIndexed(renderInstance.MeshIndexCounts_[j], 0, 0);
        }
    }

    im3dRenderer_->EndFrame();

    d3d11Context_->OMSetRenderTargets(1, &renderTargetView_, NULL);
    d3d11Context_->ClearRenderTargetView(renderTargetView_, reinterpret_cast<float*>(&clearColor));
    d3d11Context_->RSSetViewports(1, &viewport);
    ViewportsDoFrame(); //Update viewport guis which render scene view textures
    ImGuiDoFrame();

    //Present the backbuffer to the screen
    SwapChain_->Present(0, 0);
}

void DX11Renderer::HandleResize()
{
    if (!SwapChain_ || !renderTargetView_)
        return;

    UpdateWindowDimensions();

    //Cleanup swapchain resources
    ReleaseCOM(depthBuffer_);
    ReleaseCOM(depthBufferView_);
    ReleaseCOM(SwapChain_);

    //Recreate swapchain and it's resources
    if (!InitSwapchainAndResources())
        THROW_EXCEPTION("DX11Renderer::InitSwapchainAndResources() failed in DX11Renderer::HandleResize()!");

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((f32)windowWidth_, (f32)windowHeight_);
    camera_->HandleResize( { (f32)sceneViewWidth_, (f32)sceneViewHeight_} );
    im3dRenderer_->HandleResize((f32)sceneViewWidth_, (f32)sceneViewHeight_);
}

void DX11Renderer::InitTerrainMeshes(std::vector<TerrainInstance>* terrainInstances)
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
            indexBufferDesc.ByteWidth = (UINT)instance.Indices[i].size_bytes();
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
            vertexBufferDesc.ByteWidth = (UINT)instance.Vertices[i].size_bytes();
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
            renderInstance.MeshIndexCounts_[i] = (u32)instance.Indices[i].size();
            terrainModelMatrices_.push_back(DirectX::XMMATRIX());
        }

        delete instance.Indices.data();
        delete instance.Vertices.data();
        //Set bool so the instance isn't initialized more than once
        instance.RenderDataInitialized = true;
    }
}

//Todo: Toss this in a utility file somewhere
//Convert const char* to wchar_t*. Source: https://stackoverflow.com/a/8032108
const wchar_t* GetWC(const char* c)
{
    const size_t cSize = strlen(c) + 1;
    wchar_t* wc = new wchar_t[cSize];
    mbstowcs(wc, c, cSize);

    return wc;
}

void DX11Renderer::InitTerrainResources()
{
    LoadTerrainShaders();

    //Create buffer for MVP matrix constant in shader
    D3D11_BUFFER_DESC cbbd;
    ZeroMemory(&cbbd, sizeof(D3D11_BUFFER_DESC));

    //Create per object buffer
    cbbd.Usage = D3D11_USAGE_DEFAULT;
    cbbd.ByteWidth = sizeof(cbPerObject);
    cbbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbbd.CPUAccessFlags = 0;
    cbbd.MiscFlags = 0;

    HRESULT hr = d3d11Device_->CreateBuffer(&cbbd, NULL, &terrainPerObjectBuffer_);
    if (FAILED(hr))
        THROW_EXCEPTION("Failed to create terrain uniform buffer.");
}

void DX11Renderer::LoadTerrainShaders(bool reload)
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

void DX11Renderer::ViewportsDoFrame()
{
    //On first ever draw set the viewport size to the default one. Only happens if the viewport window doesn't have a .ini entry
    if (!ImGui::Begin("Scene view"))
    {
        ImGui::End();
    }
    ImVec2 contentAreaSize;
    contentAreaSize.x = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
    contentAreaSize.y = ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y;
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(clearColor.x, clearColor.y, clearColor.z, clearColor.w));

    if (contentAreaSize.x != sceneViewWidth_ || contentAreaSize.y != sceneViewHeight_)
    {
        sceneViewWidth_ = (u32)contentAreaSize.x;
        sceneViewHeight_ = (u32)contentAreaSize.y;
        InitScene();

        //Recreate depth buffer
        CreateDepthBuffer(false);
        sceneViewport_.TopLeftX = 0.0f;
        sceneViewport_.TopLeftY = 0.0f;
        sceneViewport_.Width = (f32)sceneViewWidth_;
        sceneViewport_.Height = (f32)sceneViewHeight_;
        sceneViewport_.MinDepth = 0.0f;
        sceneViewport_.MaxDepth = 1.0f;
        camera_->HandleResize({ (f32)sceneViewWidth_, (f32)sceneViewHeight_ });
        im3dRenderer_->HandleResize((f32)sceneViewWidth_, (f32)sceneViewHeight_);
    }

    //Render scene texture
    ImGui::Image(sceneViewShaderResource_, ImVec2((f32)sceneViewWidth_, (f32)sceneViewHeight_));
    ImGui::PopStyleColor();

    ImGui::End();
}

void DX11Renderer::ImGuiDoFrame()
{
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

bool DX11Renderer::InitWindow(WNDPROC wndProc)
{
    ImGui_ImplWin32_EnableDpiAwareness();

    WNDCLASSEX wc; //Create a new extended windows class

    //Todo: Move into config file or have Application class set this
    const char* windowClassName = "RF Viewer";

    wc.cbSize = sizeof(WNDCLASSEX); //Size of our windows class
    wc.style = CS_HREDRAW | CS_VREDRAW; //class styles
    wc.lpfnWndProc = wndProc; //Default windows procedure function
    wc.cbClsExtra = NULL; //Extra bytes after our wc structure
    wc.cbWndExtra = NULL; //Extra bytes after our windows instance
    wc.hInstance = hInstance_; //Instance to current application
    wc.hIcon = LoadIcon(NULL, IDI_WINLOGO); //Title bar Icon
    wc.hCursor = LoadCursor(NULL, IDC_ARROW); //Default mouse Icon
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 2); //Window bg color
    wc.lpszMenuName = NULL; //Name of the menu attached to our window
    wc.lpszClassName = windowClassName; //Name of our windows class
    wc.hIconSm = LoadIcon(NULL, IDI_WINLOGO); //Icon in your taskbar

    if (!RegisterClassEx(&wc)) //Register our windows class
    {
        //if registration failed, display error
        MessageBox(NULL, "Error registering class", "Error", MB_OK | MB_ICONERROR);
        return false;
    }

    hwnd_ = CreateWindowEx( //Create our Extended Window
        //Todo: Implement behavior for this, letting you drag and drop files onto the app. Could drop maps or packfiles onto it
        WS_EX_ACCEPTFILES, //Extended style
        windowClassName, //Name of our windows class
        "RF Viewer", //Name in the title bar of our window
        WS_OVERLAPPEDWINDOW, //style of our window
        CW_USEDEFAULT, CW_USEDEFAULT, //Top left corner of window
        windowWidth_, //Width of our window
        windowHeight_, //Height of our window
        NULL, //Handle to parent window
        NULL, //Handle to a Menu
        hInstance_, //Specifies instance of current program
        NULL //used for an MDI client window
    );

    if (!hwnd_) //Make sure our window has been created
    {
        //If not, display error
        MessageBox(NULL, "Error creating window", "Error", MB_OK | MB_ICONERROR);
        return false;
    }

    ShowWindow(hwnd_, SW_SHOW); //Shows our window
    UpdateWindow(hwnd_); //Its good to update our window

    return true; //if there were no errors, return true
}

bool DX11Renderer::InitDx11()
{
    return CreateDevice() && InitSwapchainAndResources();
}

bool DX11Renderer::InitSwapchainAndResources()
{
    bool result = CreateSwapchain() && CreateRenderTargetView() && CreateDepthBuffer();
    if (!result)
        return false;

    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = windowWidth_;
    viewport.Height = windowHeight_;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    d3d11Context_->OMSetRenderTargets(1, &sceneViewRenderTarget_, depthBufferView_);
    d3d11Context_->RSSetViewports(1, &sceneViewport_);

    return true;
}

//Init or reset texture and render target view we render the scene camera view to
bool DX11Renderer::InitScene()
{
    //Release scene texture and view if they've already been initialized
    if (sceneViewShaderResource_)
        ReleaseCOM(sceneViewShaderResource_);
    if (sceneViewRenderTarget_)
        ReleaseCOM(sceneViewRenderTarget_);
    if (sceneViewTexture_)
        ReleaseCOM(sceneViewTexture_);

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
    d3d11Device_->CreateTexture2D(&textureDesc, NULL, &sceneViewTexture_);

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

    return true;
}


bool DX11Renderer::InitImGui()
{
    //Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); 
    io.DisplaySize = ImVec2(windowWidth_, windowHeight_);
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    fontManager_->RegisterFonts();

    //Setup Dear ImGui style
    ImGui::StyleColorsDark();

    ImGui::GetStyle().FrameRounding = 4.0f;
    ImGui::GetStyle().GrabRounding = 4.0f; 

    ImVec4* colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_Text] = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.36f, 0.42f, 0.47f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
    colors[ImGuiCol_Border] = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.12f, 0.20f, 0.28f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.09f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.09f, 0.12f, 0.14f, 0.65f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.39f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.18f, 0.22f, 0.25f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.09f, 0.21f, 0.31f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.37f, 0.61f, 1.00f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.20f, 0.25f, 0.29f, 0.55f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.25f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    colors[ImGuiCol_Tab] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

    //Setup Platform/Renderer bindings
    ImGui_ImplWin32_Init(hwnd_);
    ImGui_ImplDX11_Init(d3d11Device_, d3d11Context_);

    return true;
}

bool DX11Renderer::CreateDevice()
{
    UINT createDeviceFlags = 0;

#ifdef DEBUG_BUILD
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    //Get d3d device interface ptr
    D3D_FEATURE_LEVEL featureLevel;
    HRESULT deviceCreateResult = D3D11CreateDevice(
        0, //Default adapter
        D3D_DRIVER_TYPE_HARDWARE,
        0, //No software device
        createDeviceFlags,
        0, 0, //Default feature level array
        D3D11_SDK_VERSION, &d3d11Device_, &featureLevel, &d3d11Context_);

    //Make sure it worked and is compatible with D3D11
    if (FAILED(deviceCreateResult))
    {
        MessageBox(0, "D3D11CreateDevice failed.", 0, 0);
        return false;
    }
    if (featureLevel != D3D_FEATURE_LEVEL_11_0)
    {
        MessageBox(0, "Direct3D Feature Level 11 unsupported.", 0, 0);
        return false;
    }
    //Get factory for later use. Need to use same factory instance throughout program
    if (!AcquireDxgiFactoryInstance())
    {
        return false;
    }

    return true;
}

bool DX11Renderer::CreateSwapchain()
{
    //Fill out swapchain config before creation
    DXGI_SWAP_CHAIN_DESC swapchainDesc;
    swapchainDesc.BufferDesc.Width = windowWidth_;
    swapchainDesc.BufferDesc.Height = windowHeight_;
    swapchainDesc.BufferDesc.RefreshRate.Numerator = 60; //Todo: Make configurable
    swapchainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapchainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapchainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    swapchainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchainDesc.BufferCount = 1;
    swapchainDesc.OutputWindow = hwnd_;
    swapchainDesc.Windowed = true; //Todo: Make configurable
    swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    swapchainDesc.Flags = 0;
    //Don't use MSAA
    swapchainDesc.SampleDesc.Count = 1;
    swapchainDesc.SampleDesc.Quality = 0;

    //Create swapchain
    if (FAILED(dxgiFactory_->CreateSwapChain(d3d11Device_, &swapchainDesc, &SwapChain_)))
    {
        MessageBox(0, "Failed to create swapchain!", 0, 0);
        return false;
    }

    return true;
}

bool DX11Renderer::CreateRenderTargetView()
{
    //Get ptr to swapchains backbuffer
    ID3D11Texture2D* backBuffer;
    SwapChain_->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer));

    //Create render target and get view of it
    d3d11Device_->CreateRenderTargetView(backBuffer, NULL, &renderTargetView_);

    ReleaseCOM(backBuffer);
    return true;
}

bool DX11Renderer::CreateDepthBuffer(bool firstResize)
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
    {
        MessageBox(0, "Failed to create depth buffer texture", 0, 0);
        return false;
    }
    if (FAILED(d3d11Device_->CreateDepthStencilView(depthBuffer_, 0, &depthBufferView_)))
    {
        MessageBox(0, "Failed to create depth buffer view", 0, 0);
        return false;
    }

    D3D11_BLEND_DESC blendDesc;
    ZeroMemory(&blendDesc, sizeof(blendDesc));
    blendDesc.RenderTarget[0].BlendEnable = false;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_COLOR;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_BLEND_FACTOR;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    DxCheck(d3d11Device_->CreateBlendState(&blendDesc, &blendState_), "Im3d blend state creation failed!");

    return true;
}

//Get IDXGIFactory instance from device we just created
bool DX11Renderer::AcquireDxgiFactoryInstance()
{
    //Temporary interfaces needed to get factory
    IDXGIDevice* dxgiDevice = 0;
    IDXGIAdapter* dxgiAdapter = 0;
    if (FAILED(d3d11Device_->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice))))
    {
        MessageBox(0, "Failed to get IDXGIDevice* from newly created ID3D11Device*", 0, 0);
        return false;
    }
    if (FAILED(dxgiDevice->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(&dxgiAdapter))))
    {
        MessageBox(0, "Failed to get IDXGIAdapter instance.", 0, 0);
        return false;
    }
    if (FAILED(dxgiAdapter->GetParent(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&dxgiFactory_))))
    {
        MessageBox(0, "Failed to get IDXGIFactory instance.", 0, 0);
        return false;
    }

    //Release interfaces after use. Keep factory since we'll need it longer
    ReleaseCOM(dxgiDevice);
    ReleaseCOM(dxgiAdapter);

    return true;
}

void DX11Renderer::UpdateWindowDimensions()
{
    RECT rect;
    RECT usableRect;

    if (GetClientRect(hwnd_, &usableRect))
    {
        windowWidth_ = usableRect.right - usableRect.left;
        windowHeight_ = usableRect.bottom - usableRect.top;
    }
}
