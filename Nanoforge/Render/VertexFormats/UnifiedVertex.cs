using System.Numerics;
using System.Runtime.InteropServices;

namespace Nanoforge.Render.VertexFormats;

//Primary vertex format used by the renderer. Meshes should be converted to this format by MeshConverter before being used by the renderer.
[StructLayout(LayoutKind.Sequential)]
public struct UnifiedVertex
{
    public Vector3 Position;
    public byte NormalX, NormalY, NormalZ, NormalW;
    public byte TangentX, TangentY, TangentZ, TangentW;
    public short UvX, UvY;
}