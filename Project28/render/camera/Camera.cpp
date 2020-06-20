#include "Camera.h"

Camera::Camera(const DirectX::XMVECTOR& initialPos, f32 initialFov, const DirectX::XMFLOAT2& screenDimensions, f32 nearPlane, f32 farPlane)
{
    camPosition = initialPos;
    fov_ = initialFov;
    screenDimensions_ = screenDimensions;
    aspectRatio_ = screenDimensions_.x / screenDimensions_.y;
    nearPlane_ = nearPlane;
    farPlane_ = farPlane;

    //Camera information
    camTarget = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
    //Todo: Figure out why this has to be negative for up to be the positive y axis
    camUp = DirectX::XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f);

    //Set matrices
    UpdateViewMatrix();
    UpdateProjectionMatrix();
}

void Camera::HandleResize(const DirectX::XMFLOAT2& screenDimensions)
{
    //Set the Projection matrix
    screenDimensions_ = screenDimensions;
    UpdateProjectionMatrix();
}

void Camera::UpdateProjectionMatrix()
{
    camProjection = DirectX::XMMatrixPerspectiveFovLH(fov_, screenDimensions_.x / screenDimensions_.y, nearPlane_, farPlane_);
}

void Camera::UpdateViewMatrix()
{
    camView = DirectX::XMMatrixLookAtLH(camPosition, camTarget, camUp);
}

DirectX::XMMATRIX Camera::GetViewProjMatrix()
{
    return camView * camProjection;
}
