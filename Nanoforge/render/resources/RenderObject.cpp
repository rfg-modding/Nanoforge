#include "RenderObject.h"
#include "util/Profiler.h"

void RenderObject::Draw(ComPtr<ID3D11DeviceContext> d3d11Context, Buffer& perObjectBuffer, Camera& cam)
{
    if (!Visible)
        return;

    PROFILER_FUNCTION();

    //Shader constants for this object
    PerObjectConstants constants;

    //Calculate MVP matrix for object
    DirectX::XMVECTOR rotaxis = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationAxis(rotaxis, 0.0f);
    DirectX::XMMATRIX translation = DirectX::XMMatrixTranslation(Position.x, Position.y, Position.z);
    DirectX::XMMATRIX scale = DirectX::XMMatrixScaling(Scale.x, Scale.y, Scale.z);
    //First calculate the model matrix
    constants.Model = DirectX::XMMatrixIdentity();
    constants.Model = translation * rotation * scale;
    //Then calculate model matrix with Model * View * Projection
    constants.MVP = DirectX::XMMatrixTranspose(constants.Model * cam.camView * cam.camProjection);

    constants.WorldPosition.m128_f32[0] = Position.x;
    constants.WorldPosition.m128_f32[1] = Position.y;
    constants.WorldPosition.m128_f32[2] = Position.z;
    constants.WorldPosition.m128_f32[3] = 1.0f;

    //Set MVP matrix in shader
    perObjectBuffer.SetData(d3d11Context, &constants);

    //Bind textures
    if (UseTextures)
    {
        for (u32 i = 0; i < Textures.size(); i++)
        {
            auto texture = Textures[i];
            if (texture.has_value())
                texture.value().Bind(d3d11Context, i);
        }
    }

    ObjectMesh.Draw(d3d11Context);
}