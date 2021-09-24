#pragma once
#include "common/Typedefs.h"
#include "render/resources/Mesh.h"
#include "RfgTools++/types/Vec3.h"
#include "render/camera/Camera.h"
#include "render/resources/Texture2D.h"
#include <DirectXMath.h>

//Buffer for per-object shader constants (set once per object)
struct PerObjectConstants
{
    DirectX::XMMATRIX MVP;
    DirectX::XMVECTOR WorldPosition;
};

class RenderObject
{
public:
    RenderObject(const Mesh& mesh, const Vec3& position) : ObjectMesh(mesh), Position(position), initialized_(true) { }
    //Draw the objects mesh
    void Draw(ComPtr<ID3D11DeviceContext> d3d11Context, Buffer& perObjectBuffer, Camera& cam);
    //Set uniform scale
    void SetScale(f32 scale)
    {
        Scale.x = scale;
        Scale.y = scale;
        Scale.z = scale;
    }
    bool Initialized() { return initialized_; }

    Mesh ObjectMesh;
    Vec3 Scale = { 1.0f, 1.0f, 1.0f };
    Vec3 Position = { 0.0f, 0.0f, 0.0f };
    bool Visible = true;

    bool UseTextures = false;
    std::optional<Texture2D> Texture0;
    std::optional<Texture2D> Texture1;
    std::optional<Texture2D> Texture2;
    std::optional<Texture2D> Texture3;
    std::optional<Texture2D> Texture4;
    std::optional<Texture2D> Texture5;
    std::optional<Texture2D> Texture6;
    std::optional<Texture2D> Texture7;
    std::optional<Texture2D> Texture8;
    std::optional<Texture2D> Texture9;

private:
    bool initialized_ = false;
};