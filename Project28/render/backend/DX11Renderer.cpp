#include "DX11Renderer.h"
#include <ext/WindowsWrapper.h>
#include <exception>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#include <dxgi.h>
#include <d3dcommon.h>
#include <d3d11.h>

#define ReleaseCOM(x) if(x) x->Release();

DX11Renderer::DX11Renderer(HINSTANCE hInstance, WNDPROC wndProc, int WindowWidth, int WindowHeight)
{
    hInstance_ = hInstance;
    windowWidth_ = WindowWidth;
    windowHeight_ = WindowHeight;

    if (!InitWindow(wndProc))
        throw std::exception("Failed to init window! Exiting.");
    if (!InitDx11())
        throw std::exception("Failed to init DX11! Exiting.");
    if (!InitScene())
        throw std::exception("Failed to init render scene! Exiting.");
}

DX11Renderer::~DX11Renderer()
{
    //Release DX11 resources
    ReleaseCOM(depthBuffer_);
    ReleaseCOM(depthBufferView_);
    ReleaseCOM(SwapChain_);
    ReleaseCOM(d3d11Device_);
    ReleaseCOM(d3d11Context_);
    ReleaseCOM(dxgiFactory_);
}

void DX11Renderer::DoFrame(f32 deltaTime)
{
    //Set backbuffer color
    d3d11Context_->ClearRenderTargetView(renderTargetView_, reinterpret_cast<float*>(&clearColor));

    //Present the backbuffer to the screen
    SwapChain_->Present(0, 0);
}

void DX11Renderer::HandleResize()
{
    UpdateWindowDimensions();

    //Cleanup swapchain resources
    ReleaseCOM(depthBuffer_);
    ReleaseCOM(depthBufferView_);
    ReleaseCOM(SwapChain_);

    //Recreate swapchain and it's resources
    InitSwapchainAndResources();
}

bool DX11Renderer::InitWindow(WNDPROC wndProc)
{
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

    D3D11_VIEWPORT viewport;
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
    if (GetWindowRect(hwnd_, &rect))
    {
        windowWidth_ = rect.right - rect.left;
        windowHeight_ = rect.bottom - rect.top;
    }
}
