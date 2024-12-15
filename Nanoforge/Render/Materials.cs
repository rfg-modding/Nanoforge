using System;
using System.Collections.Generic;
using Nanoforge.Render.Resources;
using Silk.NET.OpenGL;

namespace Nanoforge.Render;

public static class Materials 
{
    private static bool _initialized = false;
    private static Dictionary<string, Material> _materials { get; } = new();

    public static void Init(GL gl)
    {
        CreateMaterial(gl, "Pixlit1UvNmap", "Pixlit1UvNmap",
        [
            new VertexAttribute(VertexAttribPointerType.Float, 3),                          //Position
            new VertexAttribute(VertexAttribPointerType.UnsignedByte, 4, normalized: true), //Normal
            new VertexAttribute(VertexAttribPointerType.UnsignedByte, 4, normalized: true), //Tangent
            new VertexAttribute(VertexAttribPointerType.UnsignedShort, 2),                  //Texcoord0
        ]);
        CreateMaterial(gl, "Pixlit3UvNmap", "Pixlit3UvNmap",
        [
            new VertexAttribute(VertexAttribPointerType.Float, 3),                          //Position
            new VertexAttribute(VertexAttribPointerType.UnsignedByte, 4, normalized: true), //Normal
            new VertexAttribute(VertexAttribPointerType.UnsignedByte, 4, normalized: true), //Tangent
            new VertexAttribute(VertexAttribPointerType.UnsignedShort, 2),                  //Texcoord0
            new VertexAttribute(VertexAttribPointerType.UnsignedShort, 2),                  //Texcoord1
            new VertexAttribute(VertexAttribPointerType.UnsignedShort, 2),                  //Texcoord2
        ]);

        _initialized = true;
    }

    private static void CreateMaterial(GL gl, string materialName, string shaderName, Span<VertexAttribute> vertexAttributes)
    {
        _materials[materialName] = new Material(gl, materialName, shaderName, vertexAttributes);
    }

    public static Material GetMaterial(string materialName)
    {
        if (!_initialized)
        {
            throw new InvalidOperationException("Materials not initialized.");
        }
        return _materials[materialName];
    }

    public static void ReloadEditedShaders()
    {
        foreach (var material in _materials.Values)
        {
            material.Shader.TryReloadShaders();
        }
    }

    public static void Dispose()
    {
        foreach (var material in _materials.Values)
        {
            material.Dispose();
        }
    }
}