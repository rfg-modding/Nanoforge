using System;

namespace Nanoforge.Render;

public static class MathHelpers
{
    public static float DegreesToRadians(float degrees)
    {
        return MathF.PI / 180f * degrees;
    }

    public static float RadiansToDegrees(float pitchRadians)
    {
        return (180.0f / MathF.PI) * pitchRadians;
    }
}