using System.Numerics;
using System.Runtime.InteropServices;

namespace Nanoforge.Render.Misc;

//Size must be 16 bytes. Gets checked in Renderer constructor
[StructLayout(LayoutKind.Sequential)]
public struct ColoredVertex
{
    public Vector3 Position;
    public byte R, G, B, A;

    public ColoredVertex(Vector3 position, Vector4 color)
    {
        Position = position;
        R = (byte)(color.X * 255.0f);
        G = (byte)(color.Y * 255.0f);
        B = (byte)(color.Z * 255.0f);
        A = (byte)(color.W * 255.0f);
    }
}