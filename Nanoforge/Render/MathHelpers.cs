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

    public static float Lerp(float current, float target, float interpolant)
    {
        return current * (1.0f - interpolant) + (target * interpolant);
    }
}