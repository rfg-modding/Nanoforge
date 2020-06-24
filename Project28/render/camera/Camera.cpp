#include "Camera.h"
#include <DirectXMath.h>
#include <windowsx.h>

Camera::Camera(const DirectX::XMVECTOR& initialPos, f32 initialFov, const DirectX::XMFLOAT2& screenDimensions, f32 nearPlane, f32 farPlane)
{
    camPosition = initialPos;
    fov_ = initialFov;
    screenDimensions_ = screenDimensions;
    aspectRatio_ = screenDimensions_.x / screenDimensions_.y;
    nearPlane_ = nearPlane;
    farPlane_ = farPlane;

    //Calculate initial pitch and yaw values
    DirectX::XMVECTOR forwardVec = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract({ 0.0f, 0.0f, 0.0f }, initialPos));
    pitch_ = ToDegrees(asin(-forwardVec.m128_f32[1]));
    //Todo: Get the math right here. Subtracting from 360 is a dumb fix to get it roughly pointing at the origin.... Fine for now since only need to do this once
    //Todo: Noting for if/when the camera needs to be expanded to jump to different locations / targets in the future
    yaw_ = 360.0f - ToDegrees(acos(forwardVec.m128_f32[0]));

    //Set matrices
    UpdateViewMatrix();
    UpdateProjectionMatrix();

}

void Camera::DoFrame(f32 deltaTime)
{
    if (wDown) //w
        Translate(forward, shiftDown);
    else if (sDown) //s
        Translate(backward, shiftDown);

    if (aDown) //a
        Translate(left, shiftDown);
    else if (dDown) //d
        Translate(right, shiftDown);

    if (qDown) //q
        Translate(up, shiftDown);
    else if (eDown) //e
        Translate(down, shiftDown);
}

void Camera::HandleResize(const DirectX::XMFLOAT2& screenDimensions)
{
    //Set the Projection matrix
    screenDimensions_ = screenDimensions;
    UpdateProjectionMatrix();
}

void Camera::HandleInput(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_KEYDOWN: //Key down
        if (wParam == 0x57) //w
            wDown = true;
        if (wParam == 0x41) //a
            aDown = true;
        if (wParam == 0x53) //s
            sDown = true;
        if (wParam == 0x44) //d
            dDown = true;
        if (wParam == 0x51) //q
            qDown = true;
        if (wParam == 0x45) //e
            eDown = true;
        if (wParam == VK_SHIFT)
            shiftDown = true;

        break;
    case WM_KEYUP:
        if (wParam == 0x57) //w
            wDown = false;
        if (wParam == 0x41) //a
            aDown = false;
        if (wParam == 0x53) //s
            sDown = false;
        if (wParam == 0x44) //d
            dDown = false;
        if (wParam == 0x51) //q
            qDown = false;
        if (wParam == 0x45) //e
            eDown = false;
        if (wParam == VK_SHIFT)
            shiftDown = false;

        break;
    case WM_RBUTTONDOWN:
        rightMouseButtonDown = true;
        break;
    case WM_RBUTTONUP:
        rightMouseButtonDown = false;
        break;
    case WM_MOUSEWHEEL:
        {
            f32 scrollDelta = (f32)GET_WHEEL_DELTA_WPARAM(wParam) / 1000.0f;
            Speed += scrollDelta;
            if (Speed < MinSpeed)
                Speed = MinSpeed;
            if (Speed > MaxSpeed)
                Speed = MaxSpeed;
        }
        break;
    case WM_MOUSEMOVE:
        f32 mouseX = (f32)GET_X_LPARAM(lParam);
        f32 mouseY = (f32)GET_Y_LPARAM(lParam);
        lastMouseXDelta = mouseX - lastMouseXPos;
        lastMouseYDelta = mouseY - lastMouseYPos;
        lastMouseXPos = mouseX;
        lastMouseYPos = mouseY;
        if (rightMouseButtonDown)
            UpdateRotationFromMouse((f32)lastMouseXDelta / screenDimensions_.x, (f32)lastMouseYDelta / screenDimensions_.y);

        break;
    }
}

void Camera::UpdateProjectionMatrix()
{
    camProjection = DirectX::XMMatrixPerspectiveFovLH(fov_, screenDimensions_.x / screenDimensions_.y, nearPlane_, farPlane_);
}

void Camera::UpdateViewMatrix()
{
    //Recalculate target
    camRotationMatrix = DirectX::XMMatrixRotationRollPitchYaw(ToRadians(pitch_), ToRadians(yaw_), ToRadians(180.0f));
    camTarget = DirectX::XMVector3TransformCoord(DefaultForward, camRotationMatrix);
    camTarget = DirectX::XMVector3Normalize(camTarget);

    //Recalculate right, forward, and up vectors
    camRight = DirectX::XMVector3TransformCoord(DefaultRight, camRotationMatrix);
    camForward = DirectX::XMVector3TransformCoord(DefaultForward, camRotationMatrix);
    camUp = DirectX::XMVector3Cross(camForward, camRight);

    //Recalculate view matrix
    camTarget = DirectX::XMVectorAdd(camPosition, camTarget);
    camView = DirectX::XMMatrixLookAtLH(camPosition, camTarget, camUp);
}

DirectX::XMMATRIX Camera::GetViewProjMatrix()
{
    return camView * camProjection;
}

void Camera::Translate(const DirectX::XMVECTOR& translation)
{
    camPosition = DirectX::XMVectorAdd(camPosition, translation);
    UpdateViewMatrix();
}

void Camera::Translate(CameraDirection moveDirection, bool sprint)
{
    switch (moveDirection)
    {
    case up:
        Translate(sprint ? DirectX::XMVectorScale(camUp, SprintSpeed) : DirectX::XMVectorScale(camUp, Speed));
        break;
    case down:
        Translate(sprint ? DirectX::XMVectorScale(camUp, -SprintSpeed) : DirectX::XMVectorScale(camUp, -Speed));
        break;
    case left:
        Translate(sprint ? DirectX::XMVectorScale(Left(), SprintSpeed) : DirectX::XMVectorScale(Left(), Speed));
        break;
    case right:
        Translate(sprint ? DirectX::XMVectorScale(Right(), SprintSpeed) : DirectX::XMVectorScale(Right(), Speed));
        break;
    case forward:
        Translate(sprint ? DirectX::XMVectorScale(Forward(), SprintSpeed) : DirectX::XMVectorScale(Forward(), Speed));
        break;
    case backward:
        Translate(sprint ? DirectX::XMVectorScale(Backward(), SprintSpeed) : DirectX::XMVectorScale(Backward(), Speed));
        break;
    default:
        break;
    }
}

DirectX::XMVECTOR Camera::Up() const
{
    return camUp;
}

DirectX::XMVECTOR Camera::Down() const
{
    return DirectX::XMVectorScale(Up(), -1.0f);
}

DirectX::XMVECTOR Camera::Right() const
{
    //Todo: Figure out why this has to be negative to have right be the correct direction
    return DirectX::XMVectorSet(-camView.r[0].m128_f32[0], -camView.r[1].m128_f32[0], -camView.r[2].m128_f32[0], 1.0f);
}

DirectX::XMVECTOR Camera::Left() const
{
    return DirectX::XMVectorScale(Right(), -1.0f);
}

DirectX::XMVECTOR Camera::Forward() const
{
    return DirectX::XMVectorSet(camView.r[0].m128_f32[2], camView.r[1].m128_f32[2], camView.r[2].m128_f32[2], 1.0f);
}

DirectX::XMVECTOR Camera::Backward() const
{
    return DirectX::XMVectorScale(Forward(), -1.0f);
}

DirectX::XMVECTOR Camera::Position() const
{
    return camPosition;
}

void Camera::UpdateRotationFromMouse(f32 xDelta, f32 yDelta)
{
    //Adjust pitch and yaw
    yaw_ += xDelta * lookSensitivity_;
    pitch_ += yDelta * lookSensitivity_;

    //Limit pitch to range
    if (pitch_ > maxPitch_)
        pitch_ = maxPitch_;
    if (pitch_ < minPitch_)
        pitch_ = minPitch_;

    UpdateViewMatrix();
}

void Camera::SetPosition(f32 x, f32 y, f32 z)
{
    camPosition = DirectX::XMVectorSet(x, y, z, 1.0f);
    UpdateViewMatrix();
}

f32 Camera::ToRadians(f32 angleInDegrees)
{
    const f32 PI = 3.14159265359f;
    return angleInDegrees * (PI / 180.0f);
}

f32 Camera::ToDegrees(f32 angleInRadians)
{
    const f32 PI = 3.14159265359f;
    return (180.0f * angleInRadians) / PI;
}
