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
    constants.MVP = DirectX::XMMatrixIdentity();
    constants.MVP = translation * rotation * scale; //First calculate the model matrix
    //Then calculate model matrix with Model * View * Projection
    constants.MVP = DirectX::XMMatrixTranspose(constants.MVP * cam.camView * cam.camProjection);

    constants.WorldPosition.m128_f32[0] = Position.x;
    constants.WorldPosition.m128_f32[1] = Position.y;
    constants.WorldPosition.m128_f32[2] = Position.z;
    constants.WorldPosition.m128_f32[3] = 1.0f;

    //Set MVP matrix in shader
    perObjectBuffer.SetData(d3d11Context, &constants);

    //Bind textures
    if (UseTextures)
    {
        if (Texture0.has_value())
            Texture0.value().Bind(d3d11Context, 0);
        if (Texture1.has_value())
            Texture1.value().Bind(d3d11Context, 1);
        if (Texture2.has_value())
            Texture2.value().Bind(d3d11Context, 2);
        if (Texture3.has_value())
            Texture3.value().Bind(d3d11Context, 3);
        if (Texture4.has_value())
            Texture4.value().Bind(d3d11Context, 4);
        if (Texture5.has_value())
            Texture5.value().Bind(d3d11Context, 5);
        if (Texture6.has_value())
            Texture6.value().Bind(d3d11Context, 6);
        if (Texture7.has_value())
            Texture7.value().Bind(d3d11Context, 7);
        if (Texture8.has_value())
            Texture8.value().Bind(d3d11Context, 8);
        if (Texture9.has_value())
            Texture9.value().Bind(d3d11Context, 9);
    }

    ObjectMesh.Draw(d3d11Context);
}