#pragma once
#include "common/Typedefs.h"
#include <ext/WindowsWrapper.h>

struct IDXGIFactory;
struct IDXGISwapChain;
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Texture2D;
struct ID3D11DepthStencilView;
struct ID3D11RenderTargetView;

struct Color
{
    float red = 0.0f;
    float green = 0.0f;
    float blue = 0.0f;
    float alpha = 0.0f;
};

class DX11Renderer
{
public:
    DX11Renderer(HINSTANCE hInstance, WNDPROC wndProc, int WindowWidth, int WindowHeight);
    ~DX11Renderer();

    void DoFrame(f32 deltaTime);
    void HandleResize();

private:
    [[nodiscard]] bool InitWindow(WNDPROC wndProc);
    bool InitDx11();
    //Initialize swapchain and resources it uses. Run each time the window is resized
    bool InitSwapchainAndResources();
    bool InitScene();

    bool CreateDevice();
    bool CreateSwapchain();
    bool CreateRenderTargetView();
    bool CreateDepthBuffer();
    bool AcquireDxgiFactoryInstance();

    void UpdateWindowDimensions();

    HINSTANCE hInstance_ = nullptr;
    HWND hwnd_ = nullptr;
    int windowWidth_ = 800;
    int windowHeight_ = 800;

    IDXGIFactory* dxgiFactory_ = nullptr;
    ID3D11Device* d3d11Device_ = nullptr;
    ID3D11DeviceContext* d3d11Context_ = nullptr;
    IDXGISwapChain* SwapChain_ = nullptr;
    ID3D11RenderTargetView* renderTargetView_ = nullptr;
    ID3D11Texture2D* depthBuffer_ = nullptr;
    ID3D11DepthStencilView* depthBufferView_ = nullptr;

    int featureLevel_ = 0; //Really D3D_FEATURE_LEVEL, using int so d3d stuff only included in DX11Renderer.cpp

    Color clearColor{ 0.1f, 0.1f, 0.1f, 1.0f };
};