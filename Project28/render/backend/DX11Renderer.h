#pragma once
#include "common/Typedefs.h"
#include <ext/WindowsWrapper.h>
#include <DirectXMath.h>

struct IDXGIFactory;
struct IDXGISwapChain;
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Texture2D;
struct ID3D11DepthStencilView;
struct ID3D11RenderTargetView;
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11InputLayout;
struct ID3D11Buffer;
struct ID3D10Blob;
struct ID3D11ShaderResourceView;
struct ID3D11SamplerState;

class ImGuiFontManager;

struct Vertex
{
    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT2 texCoord;
};

class DX11Renderer
{
public:
    DX11Renderer(HINSTANCE hInstance, WNDPROC wndProc, int WindowWidth, int WindowHeight, ImGuiFontManager* fontManager);
    ~DX11Renderer();

    void DoFrame(f32 deltaTime);
    void HandleResize();

private:
    void ImGuiDoFrame();
    [[nodiscard]] bool InitWindow(WNDPROC wndProc);
    [[nodiscard]] bool InitDx11();
    //Initialize swapchain and resources it uses. Run each time the window is resized
    [[nodiscard]] bool InitSwapchainAndResources();
    [[nodiscard]] bool InitScene();
    [[nodiscard]] bool InitModels();
    [[nodiscard]] bool InitShaders();
    [[nodiscard]] bool InitImGui();


    [[nodiscard]] bool CreateDevice();
    [[nodiscard]] bool CreateSwapchain();
    [[nodiscard]] bool CreateRenderTargetView();
    [[nodiscard]] bool CreateDepthBuffer();
    [[nodiscard]] bool AcquireDxgiFactoryInstance();

    HRESULT CompileShaderFromFile(const WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3D10Blob** ppBlobOut);

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

    ID3D11VertexShader* vertexShader_ = nullptr;
    ID3D11PixelShader* pixelShader_ = nullptr;
    ID3D11InputLayout* vertexLayout_ = nullptr;
    ID3D11Buffer* vertexBuffer_ = nullptr;
    ID3D11Buffer* indexBuffer_ = nullptr;

    ImGuiFontManager* fontManager_ = nullptr;

    int featureLevel_ = 0; //Really D3D_FEATURE_LEVEL, using int so d3d stuff only included in DX11Renderer.cpp

    DirectX::XMFLOAT4 clearColor{ 0.0f, 0.0f, 0.0f, 1.0f };

    //Todo: Move camera values out into camera class
    DirectX::XMMATRIX WVP;

    DirectX::XMMATRIX cube1World;
    DirectX::XMMATRIX cube2World;

    DirectX::XMMATRIX camView;
    DirectX::XMMATRIX camProjection;

    DirectX::XMVECTOR camPosition;
    DirectX::XMVECTOR camTarget;
    DirectX::XMVECTOR camUp;

    DirectX::XMMATRIX Rotation;
    DirectX::XMMATRIX Scale;
    DirectX::XMMATRIX Translation;
    float rot = 0.01f;

    ID3D11Buffer* cbPerObjectBuffer = nullptr;
    struct cbPerObject
    {
        DirectX::XMMATRIX  WVP;
    };
    cbPerObject cbPerObj;

    ID3D11ShaderResourceView* CubesTexture = nullptr;
    ID3D11SamplerState* CubesTexSamplerState = nullptr;
};