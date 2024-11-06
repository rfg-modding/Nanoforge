using System.Numerics;

namespace Nanoforge.Math;

//TODO: Move this to the file formats project/package once that's created (or some shared package it depends on). The zone file format class and some mesh classes will need this too.
public class BoundingBox(Vector3 min, Vector3 max)
{
    public Vector3 Min = min;
    public Vector3 Max = max;

    public static BoundingBox Zero => new(Vector3.Zero, Vector3.Zero);
    
    public Vector3 Center()
    {
        Vector3 bboxSize = Max - Min;
        return Min + (bboxSize / 2);
    }

    public static BoundingBox operator +(BoundingBox bbox, Vector3 delta)
    {
        return new BoundingBox(bbox.Min + delta, bbox.Max + delta);
    }

    public bool IntersectsLine(Ray ray, ref Vector3 hit)
    {
        return IntersectsLine(ray.Start, ray.End, ref hit);
    }

    //Based on: https://stackoverflow.com/a/3235902
    public bool IntersectsLine(Vector3 lineStart, Vector3 lineEnd, ref Vector3 hit)
    {
        if (lineEnd.X < Min.X && lineStart.X < Min.X)
            return false;
        if (lineEnd.X > Max.X && lineStart.X > Max.X)
            return false;
        if (lineEnd.Y < Min.Y && lineStart.Y < Min.Y)
            return false;
        if (lineEnd.Y > Max.Y && lineStart.Y > Max.Y)
            return false;
        if (lineEnd.Z < Min.Z && lineStart.Z < Min.Z)
            return false;
        if (lineEnd.Z > Max.Z && lineStart.Z > Max.Z)
            return false;
        if (lineStart.X > Min.X && lineStart.X < Max.X &&
            lineStart.Y > Min.Y && lineStart.Y < Max.Y &&
            lineStart.Z > Min.Z && lineStart.Z < Max.Z)
        {
            hit = lineStart;
            return true;
        }

        if ((GetIntersection(lineStart.X - Min.X, lineEnd.X - Min.X, lineStart, lineEnd, ref hit) && InBox(hit, Min, Max, 1))
            || (GetIntersection(lineStart.Y - Min.Y, lineEnd.Y - Min.Y, lineStart, lineEnd, ref hit) && InBox(hit, Min, Max, 2))
            || (GetIntersection(lineStart.Z - Min.Z, lineEnd.Z - Min.Z, lineStart, lineEnd, ref hit) && InBox(hit, Min, Max, 3))
            || (GetIntersection(lineStart.X - Max.X, lineEnd.X - Max.X, lineStart, lineEnd, ref hit) && InBox(hit, Min, Max, 1))
            || (GetIntersection(lineStart.Y - Max.Y, lineEnd.Y - Max.Y, lineStart, lineEnd, ref hit) && InBox(hit, Min, Max, 2))
            || (GetIntersection(lineStart.Z - Max.Z, lineEnd.Z - Max.Z, lineStart, lineEnd, ref hit) && InBox(hit, Min, Max, 3)))
        {
            return true;
        }

        return false;
    }

    public bool GetIntersection(float dst1, float dst2, Vector3 p1, Vector3 p2, ref Vector3 hit)
    {
        if ((dst1 * dst2) >= 0.0f)
            return false;
        if (dst1 == dst2)
            return false;

        hit = p1 + (p2 - p1) * (-dst1 / (dst2 - dst1));
        return true;
    }

    public bool InBox(Vector3 hit, Vector3 b1, Vector3 b2, int axis)
    {
        if (axis == 1 && hit.Z > b1.Z && hit.Z < b2.Z && hit.Y > b1.Y && hit.Y < b2.Y)
            return true;
        if (axis == 2 && hit.Z > b1.Z && hit.Z < b2.Z && hit.X > b1.X && hit.X < b2.X)
            return true;
        if (axis == 3 && hit.X > b1.X && hit.X < b2.X && hit.Y > b1.Y && hit.Y < b2.Y)
            return true;

        return false;
    }
}