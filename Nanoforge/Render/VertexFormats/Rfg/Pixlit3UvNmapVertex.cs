using System.Numerics;

namespace Nanoforge.Render.VertexFormats.Rfg;

public struct Pixlit3UvNmapVertex
{
    public Vector3 Position;
    public byte NormalX, NormalY, NormalZ, NormalW;
    public byte TangentX, TangentY, TangentZ, TangentW;
    public short Uv0X, Uv0Y;
    public short Uv1X, Uv1Y;
    public short Uv2X, Uv2Y;
}