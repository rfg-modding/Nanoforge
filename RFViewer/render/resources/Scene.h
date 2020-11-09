#pragma once
#include "common/Typedefs.h"
#include "render/resources/Shader.h"
#include "render/resources/Texture2D.h"
#include "render/resources/Mesh.h"
#include "render/camera/Camera.h"
#include "rfg/TerrainHelpers.h"
#include <filesystem>
#include <d3d11.h>
#include <array>

//Todo: Replace with RenderObject class
//Render data for a single terrain instance. A terrain instance is the terrain for a single zone which consists of 9 meshes which are stitched together
struct TerrainInstanceRenderData
{
    std::array<Mesh, 9> Meshes = {};
    Vec3 Position;
    std::array<DirectX::XMMATRIX, 9> ModelMatrices;
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
    void HandleResize(u32 windowWidth, u32 windowHeight);
    
    void InitTerrainMeshes(std::vector<TerrainInstance>& terrainInstances);

    ID3D11ShaderResourceView* GetView() { return sceneViewTexture_.GetShaderResourceView(); }
    u32 Width() { return sceneViewWidth_; }
    u32 Height() { return sceneViewHeight_; }

    Camera Cam;
    const DirectX::XMFLOAT4 ClearColor{ 0.0f, 0.0f, 0.0f, 1.0f };

    //Todo: Get rid of this. Delete scenes like a sane person. Try again once scenes are refactored since they'll be a bit simpler
    //Dumb fix for compiler not generating copy assignment operator needed by std::vector<Scene>::erase() and bugs in own implementation
    bool Deleted = false;

private:
    void InitInternal();
    void InitRenderTarget();
    void CreateDepthBuffer();
    void InitTerrain();
    void LoadTerrainShaders();
    void ResetTerritoryData();

    //General render state
    ID3D11Device* d3d11Device_ = nullptr;
    ID3D11DeviceContext* d3d11Context_ = nullptr;
    D3D11_VIEWPORT sceneViewport_;

    //Scene state
    Texture2D sceneViewTexture_;
    Texture2D depthBufferTexture_;
    u32 sceneViewWidth_ = 200;
    u32 sceneViewHeight_ = 200;

    //TERRAIN SPECIFIC DATA
    //Todo: Mesh (1st pass):
    //Todo:     - Takes raw data, format, and other config as input and creates DX11 resources from them.
    //Todo:     - No hot reload for the moment. Just recreate texture if necessary.
    //Todo:     - Use RAII for resource management.
    //Todo:     - Put loading/saving/manipulation in util namespace or maybe static funcs

    //Todo: Re-organize helper namespaces. Likely can combine a few
    //Todo: Put loading/saving/manipulation for textures/meshes in util namespace or maybe static funcs
    //Todo: Move files into folder where useful. Likely should due to recent large amount of files added.
    //Todo: Split window code in DX11Renderer into a class or util namespace
    //Todo: Note: For first pass don't have materials, just put all shader variables in big buffer for all scene types
    //Todo: Once done make sure theres no other terrain specific code, variables, or comments in Scene

public:
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
    DirectX::XMMATRIX WVP;

    ID3D11Buffer* cbPerObjectBuffer = nullptr;
    struct cbPerObject
    {
        DirectX::XMMATRIX WVP;
    };
    cbPerObject cbPerObj;

    //Data that's the same for all terrain instances
    Shader shader_;
    ID3D11InputLayout* terrainVertexLayout_ = nullptr;
    ID3D11Buffer* terrainPerObjectBuffer_ = nullptr;
    cbPerObject cbPerObjTerrain;

    //Per instance terrain data
    std::vector<TerrainInstanceRenderData> terrainInstanceRenderData_ = {};

    //Todo: Add build path variable that's set by cmake to the project root path for debug
#ifdef DEBUG_BUILD
    string terrainShaderPath_ = "C:/Users/moneyl/source/repos/Project28/Assets/shaders/Terrain.fx";
#else
    string terrainShaderPath_ = "./Assets/shaders/Terrain.fx";
#endif
};