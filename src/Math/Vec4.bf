using System;

namespace Nanoforge.Math
{
	[Ordered]
	public struct Vec4<T> where T : operator T + T, operator T - T, operator T * T, operator T / T, operator implicit f32
						  where f32 : operator implicit T
	{
		public T x;
		public T y;
		public T z;
		public T w;

		public this()
		{
			this = default;
		}

		public this(T x, T y, T z, T w)
		{
			this.x = x;
			this.y = y;
			this.z = z;
			this.w = w;
		}

		public T Length => Math.Sqrt(x*x + y*y + z*z + w*w);
		public static Vec4<T> Zero => .(0.0f, 0.0f, 0.0f, 0.0f);

		public T Distance(Vec4<T> b)
		{
			float x = b.x - this.x;
			float y = b.y - this.y;
			float z = b.z - this.z;
			float w = b.w - this.w;
			return Math.Sqrt(x*x + y*y + z*z + w*w);
		}

		public Vec4<T> Normalized()
		{
			if (Length == 0.0f)
				return Vec4<T>(this.x, this.y, this.z, this.w);
			else
				return this / Length;
		}

		public void Normalize() mut
		{
			if (Length > 0.0f)
				this /= Length;
		}

		public static Vec4<T> operator*(Vec4<T> a, T scalar)
		{
			return .(a.x * scalar, a.y * scalar, a.z * scalar, a.w * scalar);
		}

		public static Vec4<T> operator/(Vec4<T> a, T scalar)
		{
			return .(a.x / scalar, a.y / scalar, a.z / scalar, a.w / scalar);
		}

		public void operator+=(Vec4<T> rhs) mut
		{
			x += rhs.x;
			y += rhs.y;
			z += rhs.z;
			w += rhs.w;
		}

		public void operator-=(Vec4<T> rhs) mut
		{
			x -= rhs.x;
			y -= rhs.y;
			z -= rhs.z;
			w -= rhs.w;
		}

		public void operator*=(Vec4<T> rhs) mut
		{
			x *= rhs.x;
			y *= rhs.y;
			z *= rhs.z;
			w *= rhs.w;
		}

		public void operator/=(Vec4<T> rhs) mut
		{
			x /= rhs.x;
			y /= rhs.y;
			z /= rhs.z;
			w /= rhs.w;
		}

		public static Vec4<T> operator+(Vec4<T> lhs, Vec4<T> rhs)
		{
			return .(lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w);
		}

		public static Vec4<T> operator-(Vec4<T> lhs, Vec4<T> rhs)
		{
			return .(lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.w);
		}

		public override void ToString(String strBuffer)
		{
			strBuffer.AppendF("[{}, {}, {}, {}]", x, y, z, w);
		}
	}
}
