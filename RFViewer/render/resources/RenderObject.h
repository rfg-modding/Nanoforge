#pragma once
#include "common/Typedefs.h"
#include "render/resources/Mesh.h"
#include "RfgTools++/types/Vec3.h"
#include "render/camera/Camera.h"
#include <DirectXMath.h>

//Buffer for per-object shader constants (set once per object)
struct PerObjectConstants
{
    DirectX::XMMATRIX MVP;
};

class RenderObject
{
public:
    //Create a render object
    void Create(const Mesh& mesh, const Vec3& position);
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
    Vec3 Scale;
    Vec3 Position;
};