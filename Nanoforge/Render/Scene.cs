using System.Collections.Generic;
using System.Numerics;
using Nanoforge.Gui;
using Nanoforge.Gui.Views.Controls;
using Nanoforge.Render.Resources;
using Silk.NET.Input;

namespace Nanoforge.Render;

public class Scene
{
    public List<RenderObject> RenderObjects = new();
    public Camera? Camera;

    public void Init(Vector2 viewportSize)
    {
        Camera = new(position: new Vector3(-2.5f, 3.0f, -2.5f), fovDegrees: 60.0f, viewportSize, nearPlane: 1.0f, farPlane: 10000000.0f);
    }

    public void Update(SceneFrameUpdateParams updateParams)
    {
        Camera!.Update(updateParams);
    }

    public void ViewportResize(Vector2 viewportSize)
    {
        
    }
    
    public RenderObject CreateRenderObject(string materialName, Vector3 position, Matrix4x4 orient, Mesh mesh, Texture2D texture)
    {
        RenderObject renderObject = new(position, orient, mesh, texture, materialName);
        RenderObjects.Add(renderObject);
        return renderObject;
    }

    public void Destroy()
    {
        foreach (RenderObject renderObject in RenderObjects)
        {
            renderObject.Destroy();
        }
    }
}

public struct SceneFrameUpdateParams(float deltaTime, float totalTime, bool leftMouseButtonDown, bool rightMouseButtonDown, Vector2 mousePosition, Vector2 mousePositionDelta, bool mouseOverViewport)
{
    public readonly float DeltaTime = deltaTime;
    public readonly float TotalTime = totalTime;
    public readonly MouseState Mouse = new MouseState(leftMouseButtonDown, rightMouseButtonDown, mousePosition, mousePositionDelta, mouseOverViewport);
}

public struct MouseState(bool leftMouseButtonDown, bool rightMouseButtonDown, Vector2 position, Vector2 positionDelta, bool mouseOverViewport)
{
    public readonly bool LeftMouseButtonDown = leftMouseButtonDown;
    public readonly bool RightMouseButtonDown = rightMouseButtonDown;
    public readonly Vector2 Position = position;
    public readonly Vector2 PositionDelta = positionDelta;
    public readonly bool MouseOverViewport = mouseOverViewport;
}