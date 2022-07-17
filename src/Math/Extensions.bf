using System;
using Nanoforge.Math;
using Nanoforge;

namespace System
{
	extension Math
	{
		const f64 _epsilonDouble = 2.2204460492503131e-016;

		public static bool Equal(f64 a, f64 b, f64 maxDiff = _epsilonDouble)
		{
		    return System.Math.Abs(a - b) < maxDiff;
		}

		public static f32 Range(f32 value, f32 min, f32 max)
		{
			if(value < min)
				return min;
			else if(value > max)
				return max;
			else
				return value;
		}

		public static new f32 Lerp(f32 currentVal, f32 targetVal, f32 interpolant)
		{
			return currentVal * (1.0f - interpolant) + (targetVal * interpolant);
		}

		public static Vec2<f32> Lerp(Vec2<f32> currentVal, Vec2<f32> targetVal, f32 interpolant)
		{
			return currentVal * (1.0f - interpolant) + targetVal * interpolant;
		}

		public static Mat4 Ortho(f32 left, f32 right, f32 bottom, f32 top)
		{
			Mat4 result = Mat4.Identity;
			result.Array[0] = 2.0f / (right - left);
			result.Array[5] = 2.0f / (top - bottom);
			result.Array[10] = -1.0f;
			result.Array[12] = -(right + left) / (right - left);
			result.Array[13] = -(top + bottom) / (top - bottom);
			return result;
		}

		public static Mat4 Translate(Mat4 matrix, Vec3<f32> translation)
		{
			Mat4 result = matrix;
			result.Vectors[3] = matrix.Vectors[0] * translation.x
							  + matrix.Vectors[1] * translation.y
							  + matrix.Vectors[2] * translation.z
							  + matrix.Vectors[3];
			return result;
		}

		public static Mat4 Scale(Mat4 matrix, Vec3<f32> scale)
		{
			Mat4 result = matrix;
			result.Vectors[0] = matrix.Vectors[0] * scale.x;
			result.Vectors[1] = matrix.Vectors[1] * scale.y;
			result.Vectors[2] = matrix.Vectors[2] * scale.z;
			result.Vectors[3] = matrix.Vectors[3];
			return result;
		}

		//Returns true if the position is within the rectangle
		public static bool IsPositionInRect(Vec2<f32> pos, Vec2<f32> rectPos, Vec2<f32> rectScale)
		{
			return pos.x > rectPos.x && pos.x < rectPos.x + rectScale.x && pos.y > rectPos.y && pos.y < rectPos.y + rectScale.y;
		}

		//Returns true if the two rectangles overlap
		public static bool DoRectsOverlap(Vec2<f32> rectPos0, Vec2<f32> rectSize0, Vec2<f32> rectPos1, Vec2<f32> rectSize1)
		{
			return rectPos0.x <= rectPos1.x + rectSize1.x &&
				   rectPos0.y <= rectPos1.y + rectSize1.y &&
				   rectPos1.x <= rectPos0.x + rectSize0.x &&
				   rectPos1.y <= rectPos0.y + rectSize0.y;
		}

		public static f32 ToRadians(f32 degrees)
		{
			return degrees * (Math.PI_f / 180.0f);
		}

		public static f32 ToDegrees(f32 radians)
		{
			return radians * (180.0f / Math.PI_f);
		}
	}
}
