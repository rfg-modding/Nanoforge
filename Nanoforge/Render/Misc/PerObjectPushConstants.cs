using System.Numerics;

namespace Nanoforge.Render.Misc;

public struct PerObjectPushConstants
{
    public Matrix4x4 Model;
    public Vector4 CameraPosition;
}