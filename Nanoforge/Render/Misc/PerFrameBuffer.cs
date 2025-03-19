using System.Numerics;
using System.Runtime.InteropServices;

namespace Nanoforge.Render.Misc;

[StructLayout(LayoutKind.Sequential)]
internal struct PerFrameBuffer
{
    public Matrix4x4 View;
    public Matrix4x4 Projection;
    public Vector4 CameraPosition;
}