#include "DX11Renderer.h"
#include "render/imgui/ImGuiFontManager.h"
#include "render/camera/Camera.h"
#include <ext/WindowsWrapper.h>
#include <exception>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
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

DX11Renderer::DX11Renderer(HINSTANCE hInstance, WNDPROC wndProc, int WindowWidth, int WindowHeight, ImGuiFontManager* fontManager, Camera* camera)
{
    hInstance_ = hInstance;
    windowWidth_ = WindowWidth;
    windowHeight_ = WindowHeight;
    fontManager_ = fontManager;
    camera_ = camera;
    im3dRenderer_ = new Im3dRenderer;

    if (!InitWindow(wndProc))
        throw std::exception("Failed to init window! Exiting.");

    UpdateWindowDimensions();
    if (!InitDx11())
        throw std::exception("Failed to init DX11! Exiting.");
    if (!InitScene())
        throw std::exception("Failed to init render scene! Exiting.");
    if (!InitModels())
        throw std::exception("Failed to init models! Exiting.");
    if (!InitShaders())
        throw std::exception("Failed to init shaders! Exiting.");
    if (!InitImGui())
        throw std::exception("Failed to dear imgui! Exiting.");

    im3dRenderer_->Init(d3d11Device_, d3d11Context_, hwnd_, camera_);

    //Needed by some DirectXTex functions
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    //Todo: Fix this, init should set up the matrices properly
    //Quick fix for view being distorted before first window resize
    HandleResize();
}

DX11Renderer::~DX11Renderer()
{
    im3dRenderer_->Shutdown();
    delete im3dRenderer_;

    //Shutdown dear imgui
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    //Release DX11 resources
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
    //Set render target and clear it
    d3d11Context_->OMSetRenderTargets(1, &renderTargetView_, depthBufferView_);
    d3d11Context_->ClearRenderTargetView(renderTargetView_, reinterpret_cast<float*>(&clearColor));
    d3d11Context_->ClearDepthStencilView(depthBufferView_, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    d3d11Context_->RSSetViewports(1, &viewport);
    //d3d11Context_->OMSetBlendState(blendState_, nullptr, 0xffffffff);
    d3d11Context_->OMSetDepthStencilState(depthStencilState_, 0);
    d3d11Context_->RSSetState(rasterizerState_);

    //Set the input layout
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    d3d11Context_->IASetInputLayout(vertexLayout_);
    d3d11Context_->IASetIndexBuffer(indexBuffer_, DXGI_FORMAT_R32_UINT, 0);
    d3d11Context_->IASetVertexBuffers(0, 1, &vertexBuffer_, &stride, &offset);
    d3d11Context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    //Todo: Add general mesh drawing behavior for non im3d or imgui rendering
    //Todo: Take a list of render commands each frame and draw based off of those

    im3dRenderer_->EndFrame();
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
    InitSwapchainAndResources();

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(windowWidth_, windowHeight_);

    camera_->HandleResize( { (f32)windowWidth_, (f32)windowHeight_} );
    im3dRenderer_->HandleResize((f32)windowWidth_, (f32)windowHeight_);
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
    const char* windowClassName = "Project 28";

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
        "Window Title", //Name in the title bar of our window
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

    d3d11Context_->OMSetRenderTargets(1, &renderTargetView_, depthBufferView_);
    d3d11Context_->RSSetViewports(1, &viewport);

    return true;
}

bool DX11Renderer::InitScene()
{


    return true;
}

bool DX11Renderer::InitModels()
{
    //Vertices and indices to be used
    Vertex vertices[] =
    {
        // Front Face
        {{-1.0f, -1.0f, -1.0f}, {0.0f, 1.0f}},
        {{-1.0f,  1.0f, -1.0f}, {0.0f, 0.0f}},
        {{1.0f,  1.0f, -1.0f},  {1.0f, 0.0f}},
        {{1.0f, -1.0f, -1.0f},  {1.0f, 1.0f}},

        // Back Face
        {{-1.0f, -1.0f, 1.0f}, {1.0f, 1.0f}},
        {{1.0f, -1.0f,  1.0f}, {0.0f, 1.0f}},
        {{1.0f,  1.0f,  1.0f}, {0.0f, 0.0f}},
        {{-1.0f,  1.0f, 1.0f}, {1.0f, 0.0f}},

        // Top Face
        {{-1.0f, 1.0f, -1.0f}, {0.0f, 1.0f}},
        {{-1.0f, 1.0f,  1.0f}, {0.0f, 0.0f}},
        {{1.0f, 1.0f,   1.0f}, {1.0f, 0.0f}},
        {{1.0f, 1.0f,  -1.0f}, {1.0f, 1.0f}},

        // Bottom Face
        {{-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f}},
        {{1.0f, -1.0f,  -1.0f}, {0.0f, 1.0f}},
        {{1.0f, -1.0f,   1.0f}, {0.0f, 0.0f}},
        {{-1.0f, -1.0f,  1.0f}, {1.0f, 0.0f}},

        // Left Face
        {{-1.0f, -1.0f,  1.0f}, {0.0f, 1.0f}},
        {{-1.0f,  1.0f,  1.0f}, {0.0f, 0.0f}},
        {{-1.0f,  1.0f, -1.0f}, {1.0f, 0.0f}},
        {{-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f}},

        // Right Face
        {{1.0f, -1.0f, -1.0f}, {0.0f, 1.0f}},
        {{1.0f,  1.0f, -1.0f}, {0.0f, 0.0f}},
        {{1.0f,  1.0f,  1.0f}, {1.0f, 0.0f}},
        {{1.0f, -1.0f,  1.0f}, {1.0f, 1.0f}},
    };                         

    DWORD indices[] = {
        // Front Face
        0,  1,  2,
        0,  2,  3,

        // Back Face
        4,  5,  6,
        4,  6,  7,

        // Top Face
        8,  9, 10,
        8, 10, 11,

        // Bottom Face
        12, 13, 14,
        12, 14, 15,

        // Left Face
        16, 17, 18,
        16, 18, 19,

        // Right Face
        20, 21, 22,
        20, 22, 23
    };

    //Create index buffer
    D3D11_BUFFER_DESC indexBufferDesc = {};
    indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    indexBufferDesc.ByteWidth = sizeof(DWORD) * 12 * 3;
    indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    indexBufferDesc.CPUAccessFlags = 0;
    indexBufferDesc.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA indexBufferInitData;
    indexBufferInitData.pSysMem = indices;
    HRESULT hr = d3d11Device_->CreateBuffer(&indexBufferDesc, &indexBufferInitData, &indexBuffer_);
    if (FAILED(hr))
        return false;

    d3d11Context_->IASetIndexBuffer(indexBuffer_, DXGI_FORMAT_R32_UINT, 0);

    //Create vertex buffer
    D3D11_BUFFER_DESC vertexBufferDesc = {};
    vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    vertexBufferDesc.ByteWidth = sizeof(Vertex) * 24;
    vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertexBufferDesc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA vertexBufferInitData = {};
    vertexBufferInitData.pSysMem = vertices;
    hr = d3d11Device_->CreateBuffer(&vertexBufferDesc, &vertexBufferInitData, &vertexBuffer_);
    if (FAILED(hr))
        return false;

    // Set vertex buffer
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    d3d11Context_->IASetVertexBuffers(0, 1, &vertexBuffer_, &stride, &offset);

    // Set primitive topology
    d3d11Context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    //Create buffer for MVP matrix constant in shader
    D3D11_BUFFER_DESC cbbd;
    ZeroMemory(&cbbd, sizeof(D3D11_BUFFER_DESC));

    cbbd.Usage = D3D11_USAGE_DEFAULT;
    cbbd.ByteWidth = sizeof(cbPerObject);
    cbbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbbd.CPUAccessFlags = 0;
    cbbd.MiscFlags = 0;

    hr = d3d11Device_->CreateBuffer(&cbbd, NULL, &cbPerObjectBuffer);

    //hr = D3DX11CreateShaderResourceViewFromFile(d3d11Device_, L"assets/braynzar.jpg", NULL, NULL, &CubesTexture, NULL);
    auto image = std::make_unique<DirectX::ScratchImage>();
    hr = LoadFromWICFile(L"assets/braynzar.jpg", DirectX::WIC_FLAGS::WIC_FLAGS_NONE, nullptr, *image);
    if (FAILED(hr))
        throw "Failed to load cube test texture";

    CreateShaderResourceView(d3d11Device_, image->GetImages(), image->GetImageCount(), image->GetMetadata(), &CubesTexture);

    // Describe the Sample State
    D3D11_SAMPLER_DESC sampDesc;
    ZeroMemory(&sampDesc, sizeof(sampDesc));
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

    //Create the Sample State
    hr = d3d11Device_->CreateSamplerState(&sampDesc, &CubesTexSamplerState);


    D3D11_RASTERIZER_DESC rasterizerDesc;
    ZeroMemory(&rasterizerDesc, sizeof(D3D11_RASTERIZER_DESC));
    rasterizerDesc.FillMode = D3D11_FILL_SOLID;
    rasterizerDesc.CullMode = D3D11_CULL_BACK;
    hr = d3d11Device_->CreateRasterizerState(&rasterizerDesc, &rasterizerState_);
    d3d11Context_->RSSetState(rasterizerState_);

    D3D11_DEPTH_STENCIL_DESC stencilDesc = {};
    DxCheck(d3d11Device_->CreateDepthStencilState(&stencilDesc, &depthStencilState_), "Im3d depth stencil state creation failed!");

    ////Set to render in wireframe mode
    //ID3D11RasterizerState* WireFrame;

    //D3D11_RASTERIZER_DESC wfdesc;
    //ZeroMemory(&wfdesc, sizeof(D3D11_RASTERIZER_DESC));
    //wfdesc.FillMode = D3D11_FILL_WIREFRAME;
    //wfdesc.CullMode = D3D11_CULL_NONE;
    //hr = d3d11Device_->CreateRasterizerState(&wfdesc, &WireFrame);
    //d3d11Context_->RSSetState(WireFrame);

    return true;
}

bool DX11Renderer::InitShaders()
{
    //Vertex and pixel shader binaries
    ID3DBlob* pVSBlob = nullptr;
    ID3DBlob* pPSBlob = nullptr;

    //Compile the vertex shader
    if (FAILED(CompileShaderFromFile(L"assets/shaders/Triangle.fx", "VS", "vs_4_0", &pVSBlob)))
    {
        MessageBox(nullptr, "The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", "Error", MB_OK);
        return false;
    }
    if (FAILED(d3d11Device_->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &vertexShader_)))
    {
        pVSBlob->Release();
        return false;
    }
    //Compile the pixel shader
    if (FAILED(CompileShaderFromFile(L"assets/shaders/Triangle.fx", "PS", "ps_4_0", &pPSBlob)))
    {
        MessageBox(nullptr, "The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", "Error", MB_OK);
        return false;
    }
    if (FAILED(d3d11Device_->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &pixelShader_)))
    {
        pPSBlob->Release();
        return false;
    }


    //Define the input layout
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    //Create the input layout
    if (FAILED(d3d11Device_->CreateInputLayout(layout, ARRAYSIZE(layout), pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &vertexLayout_)))
        return false;

    //Set the input layout
    d3d11Context_->IASetInputLayout(vertexLayout_);
    pVSBlob->Release();
    pPSBlob->Release();
    
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

    // Setup Dear ImGui style
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

    // Setup Platform/Renderer bindings
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

bool DX11Renderer::CreateDepthBuffer()
{
    D3D11_TEXTURE2D_DESC depthBufferDesc;
    depthBufferDesc.Width = windowWidth_;
    depthBufferDesc.Height = windowHeight_;
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
