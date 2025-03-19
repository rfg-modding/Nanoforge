using System.Collections.Generic;
using System.Numerics;
using System.Runtime.CompilerServices;
using Nanoforge.Render.Materials;
using Nanoforge.Render.Misc;
using Nanoforge.Render.Resources;
using Silk.NET.Vulkan;

namespace Nanoforge.Render;

//Use to draw primitives like lines and bounding boxes. Drawing functions must be called every frame for the primitives to continue to render.
public class PrimitiveRenderer
{
    //Linelist primitives
    private MaterialPipeline? _lineListMaterial = null;
    private List<LineVertex> _lineListVertices = new();
    private VkBuffer? _lineListVertexBuffer = null;
    
    //Solid flat shaded triangle primitives (e.g. solid bounding box)
    private MaterialPipeline? _triangleListMaterial = null;
    private List<ColoredVertex> _triangleListVertices = new();
    private VkBuffer? _triangleListVertexBuffer = null;

    //TODO: Make this and anti aliasing configurable via Cvar and gui
    public float LineWidth = 3.0f;

    public void Init(RenderContext context)
    {
        InitPrimitiveState(context);
    }

    public void Destroy()
    {
        _lineListVertexBuffer!.Destroy();
    }
    
    private void InitPrimitiveState(RenderContext context)
    {
        ulong initialPrimitiveVertices = 25000; //Initial size of primitive buffers. Made fairly large to reduce the need for resizes at runtime. Resizes cause lag when large enough.
        _lineListVertexBuffer = new VkBuffer(context, (ulong)Unsafe.SizeOf<LineVertex>() * initialPrimitiveVertices,
            BufferUsageFlags.TransferDstBit | BufferUsageFlags.VertexBufferBit, MemoryPropertyFlags.DeviceLocalBit | MemoryPropertyFlags.HostVisibleBit, canGrow: true);
        _triangleListVertexBuffer = new VkBuffer(context, (ulong)Unsafe.SizeOf<ColoredVertex>() * initialPrimitiveVertices,
            BufferUsageFlags.TransferDstBit | BufferUsageFlags.VertexBufferBit, MemoryPropertyFlags.DeviceLocalBit | MemoryPropertyFlags.HostVisibleBit, canGrow: true);

        _lineListMaterial = MaterialHelper.GetMaterialPipeline("Linelist");
        _triangleListMaterial = MaterialHelper.GetMaterialPipeline("SolidTriList");
    }

    public void RenderPrimitives(RenderContext context, CommandBuffer commandBuffer, uint frameIndex)
    {
        UpdatePrimitiveBuffers();
        DrawVertices(context, commandBuffer, frameIndex, _lineListMaterial!, _lineListVertexBuffer!, (uint)_lineListVertices.Count);
        DrawVertices(context, commandBuffer, frameIndex, _triangleListMaterial!, _triangleListVertexBuffer!, (uint)_triangleListVertices.Count);
        
        //Clear vertices for next frame
        _lineListVertices.Clear();
        _triangleListVertices.Clear();
    }

    private unsafe void DrawVertices(RenderContext context, CommandBuffer commandBuffer, uint frameIndex, MaterialPipeline pipeline, VkBuffer vertexBuffer, uint numVertices)
    {
        pipeline.Bind(commandBuffer, (int)frameIndex);
        
        var vertexBuffers = new Buffer[] { vertexBuffer.VkHandle };
        var offsets = new ulong[] { 0 };
        fixed (ulong* offsetsPtr = offsets)
        fixed (Buffer* vertexBuffersPtr = vertexBuffers)
        {
            context.Vk.CmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffersPtr, offsetsPtr);
        }

        context.Vk.CmdDraw(commandBuffer, numVertices, 1, 0, 0);
    }

    //Upload primitive vertices to GPU and resize GPU buffers if needed
    private void UpdatePrimitiveBuffers()
    {
        _lineListVertexBuffer?.SetData<LineVertex>(_lineListVertices.ToArray());
        _triangleListVertexBuffer?.SetData<ColoredVertex>(_triangleListVertices.ToArray());
    }

    public void DrawLine(Vector3 start, Vector3 end, Vector4 color, float width = 3.0f)
    {
        _lineListVertices.Add(new LineVertex(start, color, width));
        _lineListVertices.Add(new LineVertex(end, color, width));
    }

    public void DrawLine(Vector3 start, Vector3 end, Vector4 color, float widthStart = 3.0f, float widthEnd = 3.0f)
    {
        _lineListVertices.Add(new LineVertex(start, color, widthStart));
        _lineListVertices.Add(new LineVertex(end, color, widthEnd));
    }

    public void DrawQuad(Vector3 bottomLeft, Vector3 topLeft, Vector3 topRight, Vector3 bottomRight, Vector4 color)
    {
        DrawLine(bottomLeft, topLeft, color, LineWidth);
        DrawLine(topLeft, topRight, color, LineWidth);
        DrawLine(topRight, bottomRight, color, LineWidth);
        DrawLine(bottomRight, bottomLeft, color, LineWidth);
    }

    public void DrawBox(Vector3 min, Vector3 max, Vector4 color)
    {
        Vector3 size = max - min;
        Vector3 bottomLeftFront = min;
        Vector3 bottomLeftBack = min + new Vector3(size.X, 0.0f, 0.0f);
        Vector3 bottomRightFront = min + new Vector3(0.0f, 0.0f, size.Z);
        Vector3 bottomRightBack = min + new Vector3(size.X, 0.0f, size.Z);

        Vector3 topRightBack = max;
        Vector3 topLeftBack = new Vector3(bottomLeftBack.X, max.Y, bottomLeftBack.Z);
        Vector3 topRightFront = new Vector3(bottomRightFront.X, max.Y, bottomRightFront.Z);
        Vector3 topLeftFront = new Vector3(min.X, max.Y, min.Z);

        //Draw quads for the front and back faces
        DrawQuad(bottomLeftFront, topLeftFront, topRightFront, bottomRightFront, color);
        DrawQuad(bottomLeftBack, topLeftBack, topRightBack, bottomRightBack, color);

        //Draw lines connecting the two faces
        DrawLine(bottomLeftFront, bottomLeftBack, color, LineWidth);
        DrawLine(topLeftFront, topLeftBack, color, LineWidth);
        DrawLine(topRightFront, topRightBack, color, LineWidth);
        DrawLine(bottomRightFront, bottomRightBack, color, LineWidth);
    }
    
    public void DrawQuadSolid(Vector3 bottomLeft, Vector3 topLeft, Vector3 topRight, Vector3 bottomRight, Vector4 color)
    {
        //First triangle
        _triangleListVertices.Add(new ColoredVertex(bottomLeft, color));
        _triangleListVertices.Add(new ColoredVertex(topLeft, color));
        _triangleListVertices.Add(new ColoredVertex(topRight, color));

        //Second triangle
        _triangleListVertices.Add(new ColoredVertex(topRight, color));
        _triangleListVertices.Add(new ColoredVertex(bottomRight, color));
        _triangleListVertices.Add(new ColoredVertex(bottomLeft, color));
    }

    public void DrawBoxSolid(Vector3 min, Vector3 max, Vector4 color)
    {
        Vector3 size = max - min;
        Vector3 bottomLeftFront = min;
        Vector3 bottomLeftBack = min + new Vector3(size.X, 0.0f, 0.0f);
        Vector3 bottomRightFront = min + new Vector3(0.0f, 0.0f, size.Z);
        Vector3 bottomRightBack = min + new Vector3(size.X, 0.0f, size.Z);

        Vector3 topRightBack = max;
        Vector3 topLeftBack = new Vector3(bottomLeftBack.X, max.Y, bottomLeftBack.Z);
        Vector3 topRightFront = new Vector3(bottomRightFront.X, max.Y, bottomRightFront.Z);
        Vector3 topLeftFront = new Vector3(min.X, max.Y, min.Z);

        //Draw quads for each face
        DrawQuadSolid(bottomLeftFront, topLeftFront, topRightFront, bottomRightFront, color);     //Front
        DrawQuadSolid(bottomLeftBack, topLeftBack, topRightBack, bottomRightBack, color);         //Back
        DrawQuadSolid(bottomLeftBack, topLeftBack, topLeftFront, bottomLeftFront, color);         //Left
        DrawQuadSolid(bottomRightFront, topRightFront, topRightBack, bottomRightBack, color);     //Right
        DrawQuadSolid(topLeftFront, topLeftBack, topRightBack, topRightFront, color);             //Top
        DrawQuadSolid(bottomLeftFront, bottomLeftBack, bottomRightBack, bottomRightFront, color); //Bottom
    }
    
    //TODO: Port the rest of the primitive rendering functions from the Beef version of NF
}