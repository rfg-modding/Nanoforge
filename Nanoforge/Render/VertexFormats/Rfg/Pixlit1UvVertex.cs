using System.Numerics;

namespace Nanoforge.Render.VertexFormats.Rfg;

public struct Pixlit1UvVertex
{
    public Vector3 Position;
    public byte NormalX, NormalY, NormalZ, NormalW;
    public short UvX, UvY;
}