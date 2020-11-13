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
#include <imgui/backends/imgui_impl_win32.h>
#include <imgui/backends/imgui_impl_dx11.h>
#include <DirectXTex.h>
#include <Dependencies\DirectXTex\DirectXTex\DirectXTexD3D11.cpp>
#include "render/util/DX11Helpers.h"
#include "Log.h"
#include "gui/documents/PegHelpers.h"

void DX11Renderer::Init(HINSTANCE hInstance, WNDPROC wndProc, u32 WindowWidth, u32 WindowHeight, ImGuiFontManager* fontManager)
{
    hInstance_ = hInstance;
    windowWidth_ = WindowWidth;
    windowHeight_ = WindowHeight;
    fontManager_ = fontManager;

    if (!InitWindow(wndProc))
        THROW_EXCEPTION("Failed to init window! Exiting.");

    UpdateWindowDimensions();
    if (!InitDx11())
        THROW_EXCEPTION("Failed to init DX11! Exiting.");
    if (!InitImGui())
        THROW_EXCEPTION("Failed to dear imgui! Exiting.");

    //Needed by some DirectXTex functions
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr))
        THROW_EXCEPTION("CoInitializeEx() failed in DX11Renderer::Init(). Required for image/export code.");

    //Quick fix for view being distorted before first window resize
    HandleResize();

    initialized_ = true;
}

DX11Renderer::~DX11Renderer()
{
    if (!initialized_)
        return;

    //Shutdown dear imgui
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    ReleaseCOM(swapChain_);
    d3d11Device_.Reset();
    d3d11Context_.Reset();
    ReleaseCOM(dxgiFactory_);
}

void DX11Renderer::NewFrame(f32 deltaTime) const
{
    //im3dRenderer_->NewFrame(deltaTime);
}

void DX11Renderer::DoFrame(f32 deltaTime)
{
    std::lock_guard<std::mutex> lock(ContextMutex);
    d3d11Context_->OMSetDepthStencilState(depthStencilState_, 0);
    d3d11Context_->OMSetBlendState(blendState_, nullptr, 0xffffffff);
    d3d11Context_->RSSetState(rasterizerState_);

    for (auto& scene : Scenes)
        scene->Draw();

    d3d11Context_->OMSetRenderTargets(1, &renderTargetView_, nullptr);
    d3d11Context_->ClearRenderTargetView(renderTargetView_, reinterpret_cast<float*>(&clearColor));
    d3d11Context_->RSSetViewports(1, &viewport);
    ImGuiDoFrame();

    //Present the backbuffer to the screen
    swapChain_->Present(0, 0);

    ImGui::EndFrame();
}

void DX11Renderer::HandleResize()
{
    if (!swapChain_ || !renderTargetView_)
        return;

    UpdateWindowDimensions();

    //Cleanup swapchain resources
    ReleaseCOM(swapChain_);

    //Recreate swapchain and it's resources
    if (!InitSwapchainAndResources())
        THROW_EXCEPTION("DX11Renderer::InitSwapchainAndResources() failed in DX11Renderer::HandleResize()!");

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((f32)windowWidth_, (f32)windowHeight_);
}

ImTextureID DX11Renderer::TextureDataToHandle(std::span<u8> data, DXGI_FORMAT format, u32 width, u32 height)
{
    ID3D11Texture2D* texture = nullptr;
    D3D11_TEXTURE2D_DESC textureDesc;
    D3D11_SUBRESOURCE_DATA textureData;
    D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;
    ID3D11ShaderResourceView* shaderResourceView = nullptr;

    //Setup texture data subresource
    textureData.pSysMem = data.data();
    textureData.SysMemSlicePitch = 0;
    textureData.SysMemPitch = PegHelpers::CalcRowPitch(format, width, height);
    
    //Set texture description and create texture
    ZeroMemory(&textureDesc, sizeof(D3D11_TEXTURE2D_DESC));
    textureDesc.Width = width;
    textureDesc.Height = height;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.Format = format;
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    textureDesc.CPUAccessFlags = 0;
    textureDesc.MiscFlags = 0;
    if (FAILED(d3d11Device_->CreateTexture2D(&textureDesc, &textureData, &texture)))
        return nullptr;

    //Create shader resource view for texture and return it if sucessful
    ZeroMemory(&shaderResourceViewDesc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
    shaderResourceViewDesc.Format = format;
    shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    shaderResourceViewDesc.Texture2D.MipLevels = 1;
    shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
    if (FAILED(d3d11Device_->CreateShaderResourceView(texture, &shaderResourceViewDesc, &shaderResourceView)))
    {
        texture->Release();
        return nullptr;
    }
    texture->Release();

    return shaderResourceView;
}

void DX11Renderer::CreateScene()
{
    auto& scene = Scenes.emplace_back(new Scene);
    scene->Init(d3d11Device_, d3d11Context_);
}

void DX11Renderer::DeleteScene(std::shared_ptr<Scene> target)
{
    u32 index = 0;
    for (auto& scene : Scenes)
    {
        if (scene == target)
        {
            Scenes.erase(Scenes.begin() + index);
            return;
        }
        index++;
    }
    
}

void DX11Renderer::ImGuiDoFrame()
{
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

bool DX11Renderer::InitWindow(WNDPROC wndProc)
{
    ImGui_ImplWin32_EnableDpiAwareness();
    //Todo: Move into config file or have Application class set this
    const char* windowClassName = "Nanoforge";

    WNDCLASSEX wc; //Create a new extended windows class
    wc.cbSize = sizeof(WNDCLASSEX); //Size of our windows class
    wc.style = CS_HREDRAW | CS_VREDRAW; //class styles
    wc.lpfnWndProc = wndProc; //Default windows procedure function
    wc.cbClsExtra = NULL; //Extra bytes after our wc structure
    wc.cbWndExtra = NULL; //Extra bytes after our windows instance
    wc.hInstance = hInstance_; //Instance to current application
    wc.hIcon = LoadIcon(nullptr, IDI_WINLOGO); //Title bar Icon
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW); //Default mouse Icon
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 2); //Window bg color
    wc.lpszMenuName = nullptr; //Name of the menu attached to our window
    wc.lpszClassName = windowClassName; //Name of our windows class
    wc.hIconSm = LoadIcon(nullptr, IDI_WINLOGO); //Icon in your taskbar
    if (!RegisterClassEx(&wc)) //Register our windows class
        THROW_EXCEPTION("RegisterClassEx() failed in DX11Renderer::InitWindow()");

    hwnd_ = CreateWindowEx( //Create our Extended Window
        //Todo: Implement behavior for this, letting you drag and drop files onto the app. Could drop maps or packfiles onto it
        WS_EX_ACCEPTFILES, //Extended style
        windowClassName, //Name of our windows class
        "Nanoforge", //Name in the title bar of our window
        WS_OVERLAPPEDWINDOW, //style of our window
        CW_USEDEFAULT, CW_USEDEFAULT, //Top left corner of window
        windowWidth_, //Width of our window
        windowHeight_, //Height of our window
        nullptr, //Handle to parent window
        nullptr, //Handle to a Menu
        hInstance_, //Specifies instance of current program
        nullptr //used for an MDI client window
    );

    if (!hwnd_) //Make sure our window has been created
        THROW_EXCEPTION("Failed to create window in DX11Renderer::InitWindow()");

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
    const bool result = CreateSwapchain() && CreateRenderTargetView();
    if (!result)
        return false;

    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<FLOAT>(windowWidth_);
    viewport.Height = static_cast<FLOAT>(windowHeight_);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

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
    DxCheck(d3d11Device_->CreateBlendState(&blendDesc, &blendState_), "Blend state creation failed!");

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
    DxCheck(d3d11Device_->CreateRasterizerState(&rasterizerDesc, &rasterizerState_), "Rasterizer state creation failed!");

    return true;
}

bool DX11Renderer::InitImGui() const
{
    //Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); 
    io.DisplaySize = ImVec2((f32)windowWidth_, (f32)windowHeight_);
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleFonts;
    io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleViewports;

    fontManager_->RegisterFonts();

    //Setup Platform/Renderer bindings
    ImGui_ImplWin32_Init(hwnd_);
    ImGui_ImplDX11_Init(d3d11Device_.Get(), d3d11Context_.Get());

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
    const HRESULT deviceCreateResult = D3D11CreateDevice(
        0, //Default adapter
        D3D_DRIVER_TYPE_HARDWARE,
        0, //No software device
        createDeviceFlags,
        0, 0, //Default feature level array
        D3D11_SDK_VERSION, &d3d11Device_, &featureLevel, &d3d11Context_);

    //Make sure it worked and is compatible with D3D11
    if (FAILED(deviceCreateResult))
        THROW_EXCEPTION("D3D11CreateDevice failed.");
    if (featureLevel != D3D_FEATURE_LEVEL_11_0)
        THROW_EXCEPTION("Direct3D Feature Level 11 unsupported.");

    //Get factory for later use. Need to use same factory instance throughout program
    return AcquireDxgiFactoryInstance();
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
    if (FAILED(dxgiFactory_->CreateSwapChain(d3d11Device_.Get(), &swapchainDesc, &swapChain_)))
        THROW_EXCEPTION("Failed to create swapchain!");

    return true;
}

bool DX11Renderer::CreateRenderTargetView()
{
    //Get ptr to swapchains backbuffer
    ID3D11Texture2D* backBuffer;
    swapChain_->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer));
    if (!backBuffer)
        THROW_EXCEPTION("GetBuffer() returned a nullptr in DX11Renderer::CreateRenderTargetView()");

    //Create render target and get view of it
    d3d11Device_->CreateRenderTargetView(backBuffer, NULL, &renderTargetView_);

    ReleaseCOM(backBuffer);
    return true;
}

//Get IDXGIFactory instance from device we just created
bool DX11Renderer::AcquireDxgiFactoryInstance()
{
    //Temporary interfaces needed to get factory
    IDXGIDevice* dxgiDevice = 0;
    IDXGIAdapter* dxgiAdapter = 0;
    if (FAILED(d3d11Device_->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice))))
        THROW_EXCEPTION("Failed to get IDXGIDevice* from newly created ID3D11Device*");
    if (FAILED(dxgiDevice->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(&dxgiAdapter))))
        THROW_EXCEPTION("Failed to get IDXGIAdapter instance.");
    if (FAILED(dxgiAdapter->GetParent(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&dxgiFactory_))))
        THROW_EXCEPTION("Failed to get IDXGIFactory instance.");

    //Release interfaces after use. Keep factory since we'll need it longer
    ReleaseCOM(dxgiDevice);
    ReleaseCOM(dxgiAdapter);

    return true;
}

void DX11Renderer::UpdateWindowDimensions()
{
    RECT usableRect;

    if (GetClientRect(hwnd_, &usableRect))
    {
        windowWidth_ = usableRect.right - usableRect.left;
        windowHeight_ = usableRect.bottom - usableRect.top;
    }
}