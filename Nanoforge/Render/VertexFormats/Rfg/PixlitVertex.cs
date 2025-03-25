using System.Numerics;

namespace Nanoforge.Render.VertexFormats.Rfg;

public struct PixlitVertex
{
    public Vector3 Position;
    public byte NormalX, NormalY, NormalZ, NormalW;
}