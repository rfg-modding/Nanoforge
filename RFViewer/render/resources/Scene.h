#pragma once
#include "common/Typedefs.h"
#include "render/camera/Camera.h"
#include <d3d11.h>
#include "rfg/TerrainHelpers.h"
#include <array>
#include <filesystem>

//Render data for a single terrain instance. A terrain instance is the terrain for a single zone which consists of 9 meshes which are stitched together
struct TerrainInstanceRenderData
{
    std::array<ID3D11Buffer*, 9> terrainVertexBuffers_ = {};
    std::array<ID3D11Buffer*, 9> terrainIndexBuffers_ = {};
    std::array<u32, 9> MeshIndexCounts_ = {};
    Vec3 Position;
};

//Scenes represent different environments or objects that are being rendered. Each frame active scenes are rendered to a texture/render target
//which can then be used by the UI as necessary. For example, lets say you had one window with a 3d view of the campaign map, and another with sledgehammer mesh,
//each completely separate from each other. Each of those would be their own scene which are rendered to separate textures that the UI then displays.
class Scene
{
public:
    void Init(ID3D11Device* d3d11Device, ID3D11DeviceContext* d3d11Context);
    void Cleanup();
    void Draw();
    //Resizes scene render target and resources if the provided size is different than the current scene view dimensions
    void HandleResize(int windowWidth, int windowHeight);
    
    void InitTerrainMeshes(std::vector<TerrainInstance>& terrainInstances);

    ID3D11ShaderResourceView* GetView() { return sceneViewShaderResource_; }
    u32 Width() { return sceneViewWidth_; }
    u32 Height() { return sceneViewHeight_; }

    Camera Cam;
    const DirectX::XMFLOAT4 ClearColor{ 0.0f, 0.0f, 0.0f, 1.0f };

    //Todo: Get rid of this. Delete scenes like a sane person. Try again once scenes are refactored since they'll be a bit simpler
    //Dumb fix for compiler not generating copy assignment operator needed by std::vector<Scene>::erase() and bugs in own implementation
    bool Deleted = false;

    ID3D11Buffer* cbPerFrameBuffer = nullptr;
    struct cbPerFrame
    {
        DirectX::XMVECTOR ViewPos = { 0.0f, 0.0f, 0.0f, 1.0f };
        DirectX::XMVECTOR DiffuseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
        f32 DiffuseIntensity = 0.65f;
        f32 ElevationFactorBias = 0.8f;
        i32 ShadeMode = 1;
    };
    cbPerFrame cbPerFrameObject;

private:
    void InitInternal();
    void InitRenderTarget();
    void CreateDepthBuffer(bool firstResize = true);
    void InitTerrain();
    void LoadTerrainShaders(bool reload);
    void ResetTerritoryData();

    //General render state
    ID3D11Device* d3d11Device_ = nullptr;
    ID3D11DeviceContext* d3d11Context_ = nullptr;

    //Scene state
    ID3D11Texture2D* sceneViewTexture_ = nullptr;
    ID3D11RenderTargetView* sceneViewRenderTarget_ = nullptr;
    ID3D11ShaderResourceView* sceneViewShaderResource_ = nullptr;
    D3D11_VIEWPORT sceneViewport_;
    ID3D11Texture2D* depthBuffer_ = nullptr;
    ID3D11DepthStencilView* depthBufferView_ = nullptr;
    u32 sceneViewWidth_ = 200;
    u32 sceneViewHeight_ = 200;



    //TERRAIN SPECIFIC DATA
    //Todo: Move into other class / or maybe make this class virtual and specialize scenes
    //Todo: Alt: Define 'Material' class which lists out which shaders to use and the inputs for those
    //Todo:     - Plus have 'Model' and 'Texture' classes which represent other resources used

    //Todo: Make shader class that handles reload timing
    std::filesystem::file_time_type terrainShaderWriteTime_;

    //Todo: Move camera values out into camera class
    DirectX::XMMATRIX WVP;

    ID3D11Buffer* cbPerObjectBuffer = nullptr;
    struct cbPerObject
    {
        DirectX::XMMATRIX WVP;
    };
    cbPerObject cbPerObj;

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

    //Todo: Add build path variable that's set by cmake to the project root path for debug
#ifdef DEBUG_BUILD
    string terrainShaderPath_ = "C:/Users/moneyl/source/repos/Project28/Assets/shaders/Terrain.fx";
#else
    string terrainShaderPath_ = "./Assets/shaders/Terrain.fx";
#endif
};