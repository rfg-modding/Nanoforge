#pragma once
#include "common/Typedefs.h"
#include "render/resources/Mesh.h"
#include "RfgTools++/types/Vec3.h"
#include "RfgTools++/types/Mat3.h"
#include "render/camera/Camera.h"
#include "render/resources/Texture2D.h"
#include <DirectXMath.h>
#include <array>

//Buffer for per-object shader constants (set once per object)
struct PerObjectConstants
{
    DirectX::XMMATRIX MVP;
    DirectX::XMMATRIX Rotation;
    DirectX::XMVECTOR WorldPosition;
};
constexpr size_t a = sizeof(DirectX::XMMATRIX);
constexpr size_t a2 = sizeof(PerObjectConstants);

class RenderObject
{
public:
    RenderObject(const Mesh& mesh, const Vec3& position, const Mat3& rotation = {}) : ObjectMesh(mesh), Position(position), Rotation(rotation) { }
    //Draw the objects mesh
    void Draw(ComPtr<ID3D11DeviceContext> d3d11Context, Buffer& perObjectBuffer, Camera& cam);
    //Set uniform scale
    void SetScale(f32 scale)
    {
        Scale.x = scale;
        Scale.y = scale;
        Scale.z = scale;
    }

    Mesh ObjectMesh;
    Vec3 Scale = { 1.0f, 1.0f, 1.0f };
    Vec3 Position = { 0.0f, 0.0f, 0.0f };
    Mat3 Rotation = {};
    bool Visible = true;

    bool UseTextures = false;
    std::array<std::optional<Texture2D>, 10> Textures;
};