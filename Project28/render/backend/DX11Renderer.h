#pragma once
#include <ext/WindowsWrapper.h>

struct IDXGIFactory;
struct IDXGISwapChain;
struct ID3D11Device;
struct ID3D11DeviceContext;
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

    void DoFrame();

private:
    [[nodiscard]] bool InitWindow(WNDPROC wndProc);
    bool InitDx11();
    bool InitScene();

    bool CreateDevice();
    bool CreateSwapchain();
    bool CreateRenderTargetView();
    bool AcquireDxgiFactoryInstance();

    HINSTANCE hInstance_ = nullptr;
    int windowWidth_ = 800;
    int windowHeight_ = 800;
    HWND hwnd_ = nullptr;

    IDXGIFactory* dxgiFactory_ = nullptr;
    ID3D11Device* d3d11Device_ = nullptr;
    ID3D11DeviceContext* d3d11Context_ = nullptr;
    IDXGISwapChain* SwapChain_ = nullptr;
    ID3D11RenderTargetView* renderTargetView_ = nullptr;

    int featureLevel_ = 0; //Really D3D_FEATURE_LEVEL, using int so d3d stuff only included in DX11Renderer.cpp

    float red = 0.0f;
    float green = 0.0f;
    float blue = 0.0f;
    int colormodr = 1;
    int colormodg = 1;
    int colormodb = 1;
};