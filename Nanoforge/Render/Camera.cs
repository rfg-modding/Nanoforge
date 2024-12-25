using System.Linq;
using System.Numerics;
using Silk.NET.Input;

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
    private float _aspectRatio;
    private float _nearPlane;
    private float _farPlane;

    public float Speed = 25.0f;
    public float Smoothing = 0.125f;
    public float LookSensitivity = 0.01f;

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

        PitchRadians = 0.66f;
        YawRadians = 0.72f;
        UpdateViewMatrix();
        UpdateProjectionMatrix();
    }

    public void Update(float deltaTime, IInputContext input)
    {
        Position = Vector3.Lerp(Position, TargetPosition, Smoothing);

        //TODO: Update the camera to use the avalonia input system. This code still has the Silk.NET input handling from when the vulkan renderer was a standalone app.
        var keyboard = input.Keyboards.First();
        if (keyboard.IsKeyPressed(Key.W))
        {
            TargetPosition += deltaTime * Speed * Forward;
        }
        else if (keyboard.IsKeyPressed(Key.S))
        {
            TargetPosition += deltaTime * Speed * -Forward;
        }

        if (keyboard.IsKeyPressed(Key.A))
        {
            TargetPosition += deltaTime * Speed * Right;
        }
        else if (keyboard.IsKeyPressed(Key.D))
        {
            TargetPosition += deltaTime * Speed * -Right;
        }

        if (keyboard.IsKeyPressed(Key.Q))
        {
            TargetPosition.Y -= deltaTime * Speed;
        }
        else if (keyboard.IsKeyPressed(Key.E))
        {
            TargetPosition.Y += deltaTime * Speed;
        }

        UpdateViewMatrix();
        UpdateRotationFromMouse(input);
    }

    private Vector2? _lastMousePos = null;
    private void UpdateRotationFromMouse(IInputContext input)
    {
        IMouse mouse = input.Mice.First();
        if (mouse.IsButtonPressed(MouseButton.Right) && _lastMousePos.HasValue)
        {
            Vector2 mouseDelta = mouse.Position - _lastMousePos.Value;

            YawRadians += -mouseDelta.X * LookSensitivity;
            PitchRadians += mouseDelta.Y * LookSensitivity;

            float maxPitch = MathHelpers.DegreesToRadians(89.0f);
            float minPitch = MathHelpers.DegreesToRadians(-89.0f);
            
            if (PitchRadians > maxPitch)
                PitchRadians = maxPitch;
            if (PitchRadians < minPitch)
                PitchRadians = minPitch;
        }
        
        _lastMousePos = mouse.Position;
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