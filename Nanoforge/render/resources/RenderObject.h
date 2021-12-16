#pragma once
#include "common/Typedefs.h"
#include "render/resources/Mesh.h"
#include "RfgTools++/types/Vec3.h"
#include "render/camera/Camera.h"
#include "render/resources/Texture2D.h"
#include <DirectXMath.h>
#include <array>

//Buffer for per-object shader constants (set once per object)
struct PerObjectConstants
{
    DirectX::XMMATRIX MVP;
    DirectX::XMVECTOR WorldPosition;
};

class RenderObject
{
public:
    RenderObject(const Mesh& mesh, const Vec3& position) : ObjectMesh(mesh), Position(position) { }
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
    bool Visible = true;

    bool UseTextures = false;
    std::array<std::optional<Texture2D>, 10> Textures;
};