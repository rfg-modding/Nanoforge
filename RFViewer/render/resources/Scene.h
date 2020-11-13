#pragma once
#include "common/Typedefs.h"
#include "render/resources/RenderObject.h"
#include "render/resources/Texture2D.h"
#include "render/resources/Shader.h"
#include "render/resources/Buffer.h"
#include "render/resources/Mesh.h"
#include "render/camera/Camera.h"
#include "rfg/TerrainHelpers.h"
#include <filesystem>
#include <d3d11.h>
#include <array>
#include <mutex>

//Todo: Add build path variable that's set by cmake to the project root path for debug
#ifdef DEBUG_BUILD
    const string terrainShaderPath_ = "C:/Users/moneyl/source/repos/Project28/Assets/shaders/Terrain.fx";
#else
    const string terrainShaderPath_ = "./Assets/shaders/Terrain.fx";
#endif

//Buffer for per-frame shader constants (set once per frame)
struct PerFrameConstants
{
    DirectX::XMVECTOR ViewPos = { 0.0f, 0.0f, 0.0f, 1.0f };
    DirectX::XMVECTOR DiffuseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    f32 DiffuseIntensity = 0.65f;
    f32 ElevationFactorBias = 0.8f;
    i32 ShadeMode = 1;
};

//Scenes represent different environments or objects that are being rendered. Each frame active scenes are rendered to a texture/render target
//which can then be used by the UI as necessary. For example, lets say you had one window with a 3d view of the campaign map, and another with sledgehammer mesh,
//each completely separate from each other. Each of those would be their own scene which are rendered to separate textures that the UI then displays.
class Scene
{
public:
    void Init(ComPtr<ID3D11Device> d3d11Device, ComPtr<ID3D11DeviceContext> d3d11Context);
    void SetShader(const string& path);
    void SetVertexLayout(const std::vector<D3D11_INPUT_ELEMENT_DESC>& layout);
    void Draw();
    //Resizes scene render target and resources if the provided size is different than the current scene view dimensions
    void HandleResize(u32 windowWidth, u32 windowHeight);
    ID3D11ShaderResourceView* GetView() { return sceneViewTexture_.GetShaderResourceView(); }
    u32 Width() { return sceneViewWidth_; }
    u32 Height() { return sceneViewHeight_; }

    Camera Cam;
    std::vector<RenderObject> Objects = {};
    DirectX::XMFLOAT4 ClearColor{ 0.0f, 0.0f, 0.0f, 1.0f };

    //Buffer for per-frame shader constants (set once per frame)
    PerFrameConstants perFrameStagingBuffer_; //Cpu side copy of the buffer
    Buffer perFrameBuffer_; //Gpu side copy of the buffer

    //General render state
    ComPtr<ID3D11Device> d3d11Device_ = nullptr;
    ComPtr<ID3D11DeviceContext> d3d11Context_ = nullptr;

private:
    void InitInternal();
    void InitRenderTarget();
    void CreateDepthBuffer();

    //Scene state
    D3D11_VIEWPORT sceneViewport_;
    Texture2D sceneViewTexture_;
    Texture2D depthBufferTexture_;
    u32 sceneViewWidth_ = 200;
    u32 sceneViewHeight_ = 200;

    //Buffer for per-object shader constants (set once per object)
    Buffer perObjectBuffer_; //Gpu side copy of the buffer

    //Data that's the same for all terrain instances
    ComPtr<ID3D11InputLayout> vertexLayout_ = nullptr;
    Shader shader_;

    bool shaderSet_ = false;
    bool vertexLayoutSet_ = false;
};