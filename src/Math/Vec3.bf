using System;

namespace Nanoforge.Math
{
	[Ordered]
	public struct Vec3<T> where T : operator T + T, operator T - T, operator T * T, operator T / T, operator implicit f32
						  where f32 : operator implicit T
	{
		public T x;
		public T y;
		public T z;

		public this()
		{
			this = default;
		}

		public this(T x, T y, T z)
		{
			this.x = x;
			this.y = y;
			this.z = z;
		}

		public T Length => Math.Sqrt(x*x + y*y + z*z);
		public static Vec3<T> Zero => .(0.0f, 0.0f, 0.0f);

		public T Distance(Vec3<T> b)
		{
			float x = b.x - this.x;
			float y = b.y - this.y;
			float z = b.z - this.z;
			return Math.Sqrt(x*x + y*y + z*z);
		}

		public Vec3<T> Normalized()
		{
			if (Length == 0.0f)
				return Vec3<T>(this.x, this.y, this.z);
			else
				return this / Length;
		}

		public void Normalize() mut
		{
			if (Length > 0.0f)
				this /= Length;
		}

		public Vec3<T> Cross(Vec3<T> b)
		{
			return .(
					 (y * b.z) - (z * b.y),
					 (z * b.x) - (x * b.z),
					 (x * b.y) - (y * b.x)
					);
		}

		public static Vec3<T> operator*(Vec3<T> a, T scalar)
		{
			return .(a.x * scalar, a.y * scalar, a.z * scalar);
		}

		public static Vec3<T> operator/(Vec3<T> a, T scalar)
		{
			return .(a.x / scalar, a.y / scalar, a.z / scalar);
		}

		public void operator+=(Vec3<T> rhs) mut
		{
			x += rhs.x;
			y += rhs.y;
			z += rhs.z;
		}

		public void operator-=(Vec3<T> rhs) mut
		{
			x -= rhs.x;
			y -= rhs.y;
			z -= rhs.z;
		}

		public void operator*=(Vec3<T> rhs) mut
		{
			x *= rhs.x;
			y *= rhs.y;
			z *= rhs.z;
		}

		public void operator/=(Vec3<T> rhs) mut
		{
			x /= rhs.x;
			y /= rhs.y;
			z /= rhs.z;
		}

		public static Vec3<T> operator+(Vec3<T> lhs, Vec3<T> rhs)
		{
			return .(lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z);
		}

		public static Vec3<T> operator-(Vec3<T> lhs, Vec3<T> rhs)
		{
			return .(lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z);
		}

		public override void ToString(String strBuffer)
		{
			strBuffer.AppendF("[{}, {}, {}]", x, y, z);
		}
	}
}
