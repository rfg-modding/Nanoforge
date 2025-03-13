using System.Numerics;
using System.Runtime.InteropServices;

namespace Nanoforge.Render.Misc;

//Size must be 20 bytes. Gets checked in Renderer constructor
[StructLayout(LayoutKind.Sequential)]
public struct LineVertex
{
    //Position and Size positioned next to each other so they can be loaded in the vertex shader through a float4
    public Vector3 Position;
    public float Size;
    public byte R, G, B, A;

    public LineVertex(Vector3 position, Vector4 color, float size)
    {
        Position = position;
        R = (byte)(color.X * 255.0f);
        G = (byte)(color.Y * 255.0f);
        B = (byte)(color.Z * 255.0f);
        A = (byte)(color.W * 255.0f);
        Size = size;
    }
}