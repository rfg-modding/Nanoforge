using Common.Math;
using Common;
using System;

namespace Nanoforge.Misc
{
	public static class Colors
	{
		public static class RGB
		{
			public static Vec3 Red => .(1.0f, 0.0f, 0.0f);
			public static Vec3 Green => .(0.0f, 1.0f, 0.0f);
			public static Vec3 Blue => .(0.0f, 0.0f, 1.0f);
			public static Vec3 Pink => .(1.0f, 0.0f, 1.0f);
			public static Vec3 Black => .(0.0f, 0.0f, 0.0f);
			public static Vec3 White => .(1.0f, 1.0f, 1.0f);
			public static Vec3 Invisible => .(0.0f, 0.0f, 0.0f);
		}

		public static class RGBA
		{
			public static Vec4 Red => .(1.0f, 0.0f, 0.0f, 1.0f);
			public static Vec4 Green => .(0.0f, 1.0f, 0.0f, 1.0f);
			public static Vec4 Blue => .(0.0f, 0.0f, 1.0f, 1.0f);
			public static Vec4 Pink => .(1.0f, 0.0f, 1.0f, 1.0f);
			public static Vec4 Black => .(0.0f, 0.0f, 0.0f, 1.0f);
			public static Vec4 White => .(1.0f, 1.0f, 1.0f, 1.0f);
			public static Vec4 Invisible => .(0.0f, 0.0f, 0.0f, 0.0f);
		}
	}
}