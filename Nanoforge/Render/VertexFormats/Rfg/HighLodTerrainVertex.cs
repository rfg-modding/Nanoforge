namespace Nanoforge.Render.VertexFormats.Rfg;

public struct HighLodTerrainVertex
{
    public short PosX, PosY; //These aren't really X and Y. This is just the way the games shaders access the fields. X, Y, and Z are all in there.
    public byte NormalX, NormalY, NormalZ, NormalW;
}