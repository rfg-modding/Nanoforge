#include "RenderObject.h"

void RenderObject::Create(const Mesh& mesh, const Vec3& position)
{
    mesh_ = mesh;
    position_ = position;
}

void RenderObject::Draw(ComPtr<ID3D11DeviceContext> d3d11Context, Buffer& perObjectBuffer, Camera& cam)
{
    //Shader constants for this object
    PerObjectConstants constants;

    //Calculate MVP matrix for object
    DirectX::XMVECTOR rotaxis = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationAxis(rotaxis, 0.0f);
    DirectX::XMMATRIX translation = DirectX::XMMatrixTranslation(position_.x, position_.y, position_.z);
    constants.MVP = DirectX::XMMatrixIdentity();
    constants.MVP = translation * rotation; //First calculate the model matrix
    //Then calculate model matrix with Model * View * Projection
    constants.MVP = DirectX::XMMatrixTranspose(constants.MVP * cam.camView * cam.camProjection);

    //Set MVP matrix in shader
    perObjectBuffer.SetData(d3d11Context, &constants);

    //Bind objects mesh and draw it
    mesh_.Bind(d3d11Context);
    d3d11Context->DrawIndexed(mesh_.NumIndices(), 0, 0);
}