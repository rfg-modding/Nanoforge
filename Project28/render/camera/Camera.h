#pragma once
#include <Common/Typedefs.h>
#include <DirectXMath.h>

class Camera
{
public:
    Camera(const DirectX::XMVECTOR& initialPos, f32 initialFov, const DirectX::XMFLOAT2& screenDimensions, f32 nearPlane, f32 farPlane);

    void HandleResize(const DirectX::XMFLOAT2& screenDimensions);
    void UpdateProjectionMatrix();
    void UpdateViewMatrix();
    DirectX::XMMATRIX GetViewProjMatrix();

    DirectX::XMMATRIX camView;
    DirectX::XMMATRIX camProjection;

    DirectX::XMVECTOR camPosition;
    DirectX::XMVECTOR camTarget;
    DirectX::XMVECTOR camUp;

    [[nodiscard]] f32 GetFov() const { return fov_; }
    [[nodiscard]] f32 GetFovRadians() const { return fov_ * (3.141593f / 180.0f); } //Todo: Make PI a constant
    [[nodiscard]] f32 GetAspectRatio() const { return aspectRatio_; }
    [[nodiscard]] f32 GetNearPlane() const { return nearPlane_; }
    [[nodiscard]] f32 GetFarPlane() const { return farPlane_; }
    //[[nodiscard]] f32 GetLookSensitivity() const { return lookSensitivity_; }

    void SetFov(f32 fov) { fov_ = fov; UpdateProjectionMatrix(); }
    void SetNearPlane(f32 nearPlane) { nearPlane_ = nearPlane; UpdateProjectionMatrix(); }
    void SetFarPlane(f32 farPlane) { farPlane_ = farPlane; UpdateProjectionMatrix(); }
    //void SetLookSensitivity(f32 lookSensitivity) { lookSensitivity_ = lookSensitivity; }

private:
    f32 fov_ = 0.0f;
    f32 aspectRatio_ = 1.0f;
    f32 nearPlane_ = 1.0f;
    f32 farPlane_ = 100.0f;
    DirectX::XMFLOAT2 screenDimensions_ = {};
};