using System;

namespace Nanoforge.Math
{
	[Ordered][Union]
	public struct Mat3
	{
		public float[9] Array;
		public Vec3<f32>[3] Vectors;
	}
}
