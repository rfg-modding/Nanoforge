using System.Collections.Generic;
using System.Numerics;
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

    public void Update(float deltaTime, IInputContext input)
    {
        Camera!.Update(deltaTime, input);
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