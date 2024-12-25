using System.Numerics;

namespace Nanoforge.Render.Misc;

internal struct PerFrameBuffer
{
    public Matrix4x4 View;
    public Matrix4x4 Projection;
}