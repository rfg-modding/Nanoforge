using Common.Math;
using Common;
using System;

namespace Nanoforge.Misc
{
	public static class Colors
	{
		public static class RGB
		{
			public static Vec3<f32> Red => .(1.0f, 0.0f, 0.0f);
			public static Vec3<f32> Green => .(0.0f, 1.0f, 0.0f);
			public static Vec3<f32> Blue => .(0.0f, 0.0f, 1.0f);
			public static Vec3<f32> Pink => .(1.0f, 0.0f, 1.0f);
			public static Vec3<f32> Black => .(0.0f, 0.0f, 0.0f);
			public static Vec3<f32> White => .(1.0f, 1.0f, 1.0f);
			public static Vec3<f32> Invisible => .(0.0f, 0.0f, 0.0f);
		}

		public static class RGBA
		{
			public static Vec4<f32> Red => .(1.0f, 0.0f, 0.0f, 1.0f);
			public static Vec4<f32> Green => .(0.0f, 1.0f, 0.0f, 1.0f);
			public static Vec4<f32> Blue => .(0.0f, 0.0f, 1.0f, 1.0f);
			public static Vec4<f32> Pink => .(1.0f, 0.0f, 1.0f, 1.0f);
			public static Vec4<f32> Black => .(0.0f, 0.0f, 0.0f, 1.0f);
			public static Vec4<f32> White => .(1.0f, 1.0f, 1.0f, 1.0f);
			public static Vec4<f32> Invisible => .(0.0f, 0.0f, 0.0f, 0.0f);
		}
	}
}