using System;

namespace Nanoforge.Math
{
	[Ordered][Union]
	public struct Mat4
	{
		public float[16] Array;
		public Vec4<f32>[4] Vectors;

		//Set to identity matrix by default
		public this()
		{
			this.Array[0] = 1.0f;
			this.Array[1] = 0.0f;
			this.Array[2] = 0.0f;
			this.Array[3] = 0.0f;

			this.Array[4] = 0.0f;
			this.Array[5] = 1.0f;
			this.Array[6] = 0.0f;
			this.Array[7] = 0.0f;

			this.Array[8] = 0.0f;
			this.Array[9] = 0.0f;
			this.Array[10] = 1.0f;
			this.Array[11] = 0.0f;

			this.Array[12] = 0.0f;
			this.Array[13] = 0.0f;
			this.Array[14] = 0.0f;
			this.Array[15] = 1.0f;
		}
		public static Mat4 Identity = .();

		public static Mat4 operator*(Mat4 lhs, Mat4 rhs)
		{
			Vec4<f32> srcA0 = lhs.Vectors[0];
			Vec4<f32> srcA1 = lhs.Vectors[1];
			Vec4<f32> srcA2 = lhs.Vectors[2];
			Vec4<f32> srcA3 = lhs.Vectors[3];

			Vec4<f32> srcB0 = rhs.Vectors[0];
			Vec4<f32> srcB1 = rhs.Vectors[1];
			Vec4<f32> srcB2 = rhs.Vectors[2];
			Vec4<f32> srcB3 = rhs.Vectors[3];

			Mat4 result;
			result.Vectors[0] = srcA0 * srcB0.x + srcA1 * srcB0.y + srcA2 * srcB0.z + srcA3 * srcB0.w;
			result.Vectors[1] = srcA0 * srcB1.x + srcA1 * srcB1.y + srcA2 * srcB1.z + srcA3 * srcB1.w;
			result.Vectors[2] = srcA0 * srcB2.x + srcA1 * srcB2.y + srcA2 * srcB2.z + srcA3 * srcB2.w;
			result.Vectors[3] = srcA0 * srcB3.x + srcA1 * srcB3.y + srcA2 * srcB3.z + srcA3 * srcB3.w;

			return result;
		}
	}
}
