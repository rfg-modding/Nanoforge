using System;

namespace Nanoforge.Math
{
	[Ordered][Union]
	public struct Mat2
	{
		public float[4] Array;
		public Vec2<f32>[2] Vectors;
	}
}
