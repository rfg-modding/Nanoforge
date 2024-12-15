using System;
using Silk.NET.OpenGL;

namespace Nanoforge.Render.Resources;

public class Material : IDisposable
{
    public string Name;
    public string ShaderPath;
    public readonly VertexAttribute[] VertexAttributes;
    public Shader Shader;
    public Material(GL gl, string name, string shaderPath, Span<VertexAttribute> vertexAttributes)
    {
        string shaderDirectory = "/home/moneyl/projects/Nanoforge/Nanoforge/assets/shaders/";
        
        Name = name;
        ShaderPath = shaderPath;
        VertexAttributes = vertexAttributes.ToArray();
        
        string vertexPath = $"{shaderDirectory}{shaderPath}.vert";
        string fragmentPath = $"{shaderDirectory}{shaderPath}.frag";
        Shader = new Shader(gl, vertexPath, fragmentPath);
    }
    
    public unsafe uint MakeVAO(GL gl, uint vbo, uint ebo)
    {
        uint vao = gl.GenVertexArray();
        gl.BindVertexArray(vao);
        gl.BindBuffer(BufferTargetARB.ArrayBuffer, vbo); //TODO: Consider switching this to use the Buffer class + its bind function
        gl.BindBuffer(BufferTargetARB.ElementArrayBuffer, ebo);
        
        uint totalVertexSize = 0;
        foreach (VertexAttribute attribute in VertexAttributes)
        {
            totalVertexSize += attribute.Size();
        }
        
        uint attributeIndex = 0;
        uint offset = 0;
        foreach (VertexAttribute attribute in VertexAttributes)
        {
            gl.EnableVertexAttribArray(attributeIndex);
            gl.VertexAttribPointer(attributeIndex, attribute.Count, attribute.AttributeType, attribute.Normalized, totalVertexSize, (void*)offset);
            offset += attribute.Size();
            attributeIndex++;
        }
        
        return vao;
    }

    public void Dispose()
    {
        Shader.Dispose();
    }
}

public struct VertexAttribute(VertexAttribPointerType attributeType, int count, bool normalized = false)
{
    public VertexAttribPointerType AttributeType = attributeType;
    public int Count = count;
    public bool Normalized = normalized;

    public uint Size()
    {
        return (uint)Count * TypeSize();
    }
    
    public uint TypeSize()
    {
        switch (AttributeType)
        {
            case VertexAttribPointerType.Byte:
                return 1;
            case VertexAttribPointerType.UnsignedByte:
                return 1;
            case VertexAttribPointerType.Short:
                return 2;
            case VertexAttribPointerType.UnsignedShort:
                return 2;
            case VertexAttribPointerType.Int:
                return 4;
            case VertexAttribPointerType.UnsignedInt:
                return 4;
            case VertexAttribPointerType.Float:
                return 4;
            case VertexAttribPointerType.Double:
                return 8;
            case VertexAttribPointerType.HalfFloat:
            case VertexAttribPointerType.Fixed:
            case VertexAttribPointerType.Int64Arb:
            case VertexAttribPointerType.UnsignedInt64Arb:
            case VertexAttribPointerType.UnsignedInt2101010Rev:
            case VertexAttribPointerType.UnsignedInt10f11f11fRev:
            case VertexAttribPointerType.Int2101010Rev:
            default:
                throw new ArgumentOutOfRangeException(nameof(AttributeType), AttributeType, null);
        }
    }
}
