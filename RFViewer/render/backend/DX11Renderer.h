#pragma once
#include "common/Typedefs.h"
#include "rfg/TerrainHelpers.h"
#include <ext/WindowsWrapper.h>
#include <DirectXMath.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <array>

class ImGuiFontManager;
class Camera;
class Im3dRenderer;

struct Vertex
{
    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT2 texCoord;
};

//Render data for a single terrain instance. A terrain instance is the terrain for a single zone which consists of 9 meshes which are stitched together
struct TerrainInstanceRenderData
{
    std::array<ID3D11Buffer*, 9> terrainVertexBuffers_ = {};
    std::array<ID3D11Buffer*, 9> terrainIndexBuffers_ = {};
    std::array<u32, 9> MeshIndexCounts_ = {};
    Vec3 Position;
};

class DX11Renderer
{
public:
    void Init(HINSTANCE hInstance, WNDPROC wndProc, int WindowWidth, int WindowHeight, ImGuiFontManager* fontManager, Camera* camera);
    ~DX11Renderer();

    void NewFrame(f32 deltaTime);
    void DoFrame(f32 deltaTime);
    void HandleResize();
    HWND GetSystemWindowHandle() { return hwnd_; }

    void InitTerrainMeshes(std::vector<TerrainInstance>* terrainInstances);

private:
    void InitTerrainResources();
    //Todo: Add a callback so viewport windows can be written outside of the renderer
    //Draw viewports which render a DX11 texture (usually of a scene)
    void ViewportsDoFrame();
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
    [[nodiscard]] bool CreateDepthBuffer(bool firstResize = true);
    [[nodiscard]] bool AcquireDxgiFactoryInstance();

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
    Camera* camera_ = nullptr;
    Im3dRenderer* im3dRenderer_ = nullptr;

    D3D_FEATURE_LEVEL featureLevel_;

    DirectX::XMFLOAT4 clearColor{ 0.0f, 0.0f, 0.0f, 1.0f };

    //Todo: Move camera values out into camera class
    DirectX::XMMATRIX WVP;

    DirectX::XMMATRIX cube1World;
    DirectX::XMMATRIX cube2World;

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

    ID3D11SamplerState* CubesTexSamplerState = nullptr;

    ID3D11RasterizerState* rasterizerState_ = nullptr;
    ID3D11BlendState* blendState_ = nullptr;
    ID3D11DepthStencilState* depthStencilState_ = nullptr;
    D3D11_VIEWPORT viewport;

    //Terrain data
    //Data that's the same for all terrain instances
    ID3D11VertexShader* terrainVertexShader_ = nullptr;
    ID3D11PixelShader* terrainPixelShader_ = nullptr;
    ID3D11InputLayout* terrainVertexLayout_ = nullptr;
    ID3D11Buffer* terrainPerObjectBuffer_ = nullptr;
    cbPerObject cbPerObjTerrain;
    UINT terrainVertexStride = 0;
    UINT terrainVertexOffset = 0;

    //Per instance terrain data
    std::vector<TerrainInstanceRenderData> terrainInstanceRenderData_ = {};
    std::vector<DirectX::XMMATRIX> terrainModelMatrices_ = {};

    //Todo: Make general scene and camera classes and support multiple scenes & camera views which might be rendered to an imgui window
    //Variables for rendering main scene camera to a texture
    ID3D11Texture2D* sceneViewTexture_ = nullptr;
    ID3D11RenderTargetView* sceneViewRenderTarget_ = nullptr;
    ID3D11ShaderResourceView* sceneViewShaderResource_ = nullptr;
    D3D11_VIEWPORT sceneViewport_;
    u32 sceneViewWidth_ = 200;
    u32 sceneViewHeight_ = 200;
};