using System.Numerics;

namespace Nanoforge.Render.VertexFormats.Rfg;

public struct Pixlit1UvNmapVertex
{
    public Vector3 Position;
    public byte NormalX, NormalY, NormalZ, NormalW;
    public byte TangentX, TangentY, TangentZ, TangentW;
    public short UvX, UvY;
}