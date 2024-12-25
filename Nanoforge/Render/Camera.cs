using System;
using System.Linq;
using System.Numerics;
using Avalonia.Input;
using Nanoforge.Gui;
using Nanoforge.Gui.Views;
using Nanoforge.Gui.Views.Controls;

namespace Nanoforge.Render;

public class Camera
{
    public Vector3 Position;
    public Vector3 TargetPosition;
    
    public Matrix4x4 View;
    public Matrix4x4 Projection;

    public float FovRadians = 60.0f;
    public float PitchRadians;
    public float YawRadians;
    public float TargetPitchRadians;
    public float TargetYawRadians;
    
    private float _aspectRatio;
    private float _nearPlane;
    private float _farPlane;

    public float Speed = 25.0f;
    public float MovementSmoothing = 0.125f;
    public float LookSensitivity = 0.03f;
    public float LookSmoothing = 0.4f;

    public Vector3 Forward;
    public Vector3 Up;
    public Vector3 Right;

    public Camera(Vector3 position, float fovDegrees, Vector2 viewportSize, float nearPlane, float farPlane)
    {
        Position = position;
        TargetPosition = position;
        FovRadians = MathHelpers.DegreesToRadians(fovDegrees);
        _aspectRatio = viewportSize.X / viewportSize.Y;
        _nearPlane = nearPlane;
        _farPlane = farPlane;
        
        Forward = Vector3.Normalize(-position);
        Up = Vector3.UnitY;
        Right = Vector3.Cross(Up, Forward);

        TargetPitchRadians = PitchRadians = 0.66f;
        TargetYawRadians = YawRadians = 0.72f;
        UpdateViewMatrix();
        UpdateProjectionMatrix();
    }

    public void Update(SceneFrameUpdateParams updateParams)
    {
        Position = Vector3.Lerp(Position, TargetPosition, MovementSmoothing);

        Input input = (MainWindow.Instance as MainWindow)!.Input;
        if (input.IsKeyDown(Key.W))
        {
            TargetPosition += updateParams.DeltaTime * Speed * Forward;
        }
        else if (input.IsKeyDown(Key.S))
        {
            TargetPosition += updateParams.DeltaTime * Speed * -Forward;
        }

        if (input.IsKeyDown(Key.A))
        {
            TargetPosition += updateParams.DeltaTime * Speed * Right;
        }
        else if (input.IsKeyDown(Key.D))
        {
            TargetPosition += updateParams.DeltaTime * Speed * -Right;
        }

        if (input.IsKeyDown(Key.Q))
        {
            TargetPosition.Y -= updateParams.DeltaTime * Speed;
        }
        else if (input.IsKeyDown(Key.E))
        {
            TargetPosition.Y += updateParams.DeltaTime * Speed;
        }

        
        UpdateRotationFromMouse(updateParams);
        YawRadians = MathHelpers.Lerp(YawRadians, TargetYawRadians, LookSmoothing); 
        PitchRadians = MathHelpers.Lerp(PitchRadians, TargetPitchRadians, LookSmoothing);
        
        UpdateViewMatrix();
    }

    private void UpdateRotationFromMouse(SceneFrameUpdateParams updateParams)
    {
        MouseState mouse = updateParams.Mouse;
        if (mouse.RightMouseButtonDown)
        {
            TargetYawRadians += -mouse.PositionDelta.X * LookSensitivity;
            TargetPitchRadians += mouse.PositionDelta.Y * LookSensitivity;

            float maxPitch = MathHelpers.DegreesToRadians(89.0f);
            float minPitch = MathHelpers.DegreesToRadians(-89.0f);
            
            if (TargetPitchRadians > maxPitch)
                TargetPitchRadians = maxPitch;
            if (TargetPitchRadians < minPitch)
                TargetPitchRadians = minPitch;
        }
    }

    public void UpdateViewMatrix()
    {
        Vector3 defaultForward = new Vector3(0.0f, 0.0f, 1.0f);
        Vector3 defaultRight = new Vector3(1.0f, 0.0f, 0.0f);
        
        Matrix4x4 rotation = Matrix4x4.CreateFromYawPitchRoll(YawRadians, PitchRadians, roll: 0.0f);
        
        Right = Transform(defaultRight, rotation);
        Forward = Transform(defaultForward, rotation);
        Up = Vector3.Cross(Forward, Right);

        Vector3 focus = Position + Vector3.Normalize(Transform(defaultForward, rotation));
        View  = Matrix4x4.CreateLookAt(Position, focus, Up);
    }
    
    private Vector3 Transform(Vector3 v, Matrix4x4 transform)
    {
        Vector4 result = new Vector4(transform.M41, transform.M42, transform.M43, transform.M44) + 
                         (v.X * new Vector4(transform.M11, transform.M12, transform.M13, transform.M14)) + 
                         (v.Y * new Vector4(transform.M21, transform.M22, transform.M23, transform.M24)) + 
                         (v.Z * new Vector4(transform.M31, transform.M32, transform.M33, transform.M34));
        
        result /= result.W;
        return new Vector3(result.X, result.Y, result.Z);
    }

    public void UpdateProjectionMatrix()
    {
        Projection = Matrix4x4.CreatePerspectiveFieldOfView(FovRadians, _aspectRatio, _nearPlane, _farPlane);
        Projection.M22 *= -1;
    }
    
    public void ViewportResize(Vector2 viewportSize)
    {
        _aspectRatio = viewportSize.X / viewportSize.Y;
    }
}