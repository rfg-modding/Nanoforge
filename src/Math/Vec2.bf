using System;

namespace Nanoforge.Math
{
	[Ordered]
	public struct Vec2<T> where T : operator T + T, operator T &- T, operator T * T, operator T / T, operator explicit f32
						  where f32 : operator implicit T
	{
		public T x;
		public T y;

		public this()
		{
			this = default;
		}

		public this(T x, T y)
		{
			this.x = x;
			this.y = y;
		}

		public T Length => (T)Math.Sqrt(x*x + y*y);

		public T Distance(Vec2<T> b)
		{
			float x = b.x &- this.x;
			float y = b.y &- this.y;
			return (T)Math.Sqrt(x*x + y*y);
		}

		public Vec2<T> Normalized()
		{
			if (Length == 0.0f)
				return Vec2<T>(this.x, this.y);
			else
				return this / Length;
		}

		public void Normalize() mut
		{
			if (Length > 0.0f)
				this /= Length;
		}

		public static Vec2<T> operator*(Vec2<T> a, T scalar)
		{
			return .(a.x * scalar, a.y * scalar);
		}

		public static Vec2<T> operator/(Vec2<T> a, T scalar)
		{
			return .(a.x / scalar, a.y / scalar);
		}

		public void operator+=(Vec2<T> rhs) mut
		{
			x += rhs.x;
			y += rhs.y;
		}

		public void operator-=(Vec2<T> rhs) mut
		{
			x &-= rhs.x;
			y &-= rhs.y;
		}

		public void operator*=(Vec2<T> rhs) mut
		{
			x *= rhs.x;
			y *= rhs.y;
		}

		public void operator/=(Vec2<T> rhs) mut
		{
			x /= rhs.x;
			y /= rhs.y;
		}

		public static Vec2<T> operator+(Vec2<T> lhs, Vec2<T> rhs)
		{
			return .(lhs.x + rhs.x, lhs.y + rhs.y);
		}

		public static Vec2<T> operator-(Vec2<T> lhs, Vec2<T> rhs)
		{
			return .(lhs.x &- rhs.x, lhs.y &- rhs.y);
		}

		public static Vec2<T> Zero => .((T)0, (T)0);

		public override void ToString(String strBuffer)
		{
			strBuffer.AppendF("[{}, {}]", x, y);
		}
	}
}
