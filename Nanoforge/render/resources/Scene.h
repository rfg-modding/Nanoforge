#pragma once
#include "common/Typedefs.h"
#include "render/resources/RenderObject.h"
#include "render/resources/Texture2D.h"
#include "render/resources/Material.h"
#include "render/resources/Shader.h"
#include "render/resources/Buffer.h"
#include "render/resources/Mesh.h"
#include "render/camera/Camera.h"
#include "rfg/TerrainHelpers.h"
#include "RfgTools++/types/Vec2.h"
#include <filesystem>
#include <d3d11.h>
#include <array>
#include <mutex>

class Config;

//Buffer for per-frame shader constants (set once per frame)
struct PerFrameConstants
{
    DirectX::XMVECTOR ViewPos = { 0.0f, 0.0f, 0.0f, 1.0f };
    DirectX::XMVECTOR DiffuseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    f32 DiffuseIntensity = 0.65f;
    f32 ElevationFactorBias = 0.8f;
    i32 ShadeMode = 1;
    f32 Time = 0.0f;
    Vec2 ViewportDimensions;
};
const Vec3 ColorWhite(1.0f, 1.0f, 1.0f);

//Scenes represent different environments or objects that are being rendered. Each frame active scenes are rendered to a texture/render target
//which can then be used by the UI as necessary. For example, lets say you had one window with a 3d view of the campaign map, and another with sledgehammer mesh,
//each completely separate from each other. Each of those would be their own scene which are rendered to separate textures that the UI then displays.
class Scene
{
public:
    void Init(ComPtr<ID3D11Device> d3d11Device, ComPtr<ID3D11DeviceContext> d3d11Context, Config* config);
    void Draw(f32 deltaTime);
    //Resizes scene render target and resources if the provided size is different than the current scene view dimensions
    void HandleResize(u32 windowWidth, u32 windowHeight);
    ID3D11ShaderResourceView* GetView() { return sceneViewTexture_.GetShaderResourceView(); }
    u32 Width() { return sceneViewWidth_; }
    u32 Height() { return sceneViewHeight_; }

    //Primitive drawing functions. Must be called each frame
    void DrawLine(const Vec3& start, const Vec3& end, const Vec3& color = ColorWhite);
    void DrawBox(const Vec3& min, const Vec3& max, const Vec3& color = ColorWhite);
    //Clear any existing primitives and force the primitive vertex buffers to be updated
    void ResetPrimitives();

    Handle<RenderObject> CreateRenderObject();

    Camera Cam;
    std::vector<Handle<RenderObject>> Objects = {};
    std::mutex ObjectCreationMutex;
    DirectX::XMFLOAT4 ClearColor{ 0.0f, 0.0f, 0.0f, 1.0f };
    f32 TotalTime = 0.0f; //Total frame time

    //Buffer for per-frame shader constants (set once per frame)
    PerFrameConstants perFrameStagingBuffer_; //Cpu side copy of the buffer
    Buffer perFrameBuffer_; //Gpu side copy of the buffer

    //General render state
    ComPtr<ID3D11Device> d3d11Device_ = nullptr;
    ComPtr<ID3D11DeviceContext> d3d11Context_ = nullptr;

    Material material;

    bool NeedsRedraw = true;

private:
    void InitInternal();
    void InitPrimitiveState();
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

    //Vertex layout used by all meshes in this scene
    ComPtr<ID3D11RasterizerState> meshRasterizerState_ = nullptr;

    //Data used for drawing primitives
    struct ColoredVertex //Used by primitives that need different colors per vertex
    {
        Vec3 Position;
        Vec3 Color;
    };

    ComPtr<ID3D11RasterizerState> primitiveRasterizerState_ = nullptr;
    Material linelistMaterial_;

    //Primitive drawing temporary buffers. Cleared each frame
    //Line and linestrip data. Lines are also used to draw boxes
    std::vector<ColoredVertex> lineVertices_;
    Buffer lineVertexBuffer_;
    u32 numLineVertices_ = 0;
    bool primitiveBufferNeedsUpdate_ = true;

    Config* config_ = nullptr;
    bool errorOccurred_ = false;
};