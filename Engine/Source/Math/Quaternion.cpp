#include "RdPch.h"
#include "Math/Quaternion.h"
#include "Math/Matrix4.h"

namespace rd
{
	Quaternion Quaternion::AngleAxis(const Vector3& axis, float radians)
	{
		Vector3 n = Normalize(axis);
		float h = radians * 0.5f;
		float s = std::sin(h);
		return { std::cos(h), n.x * s, n.y * s, n.z * s };
	}

	Quaternion Quaternion::FromEuler(const Vector3& e)
	{
		float cx = std::cos(e.x * 0.5f), sx = std::sin(e.x * 0.5f);
		float cy = std::cos(e.y * 0.5f), sy = std::sin(e.y * 0.5f);
		float cz = std::cos(e.z * 0.5f), sz = std::sin(e.z * 0.5f);
		return Normalize(Quaternion(
			cz * cy * cx + sz * sy * sx,
			cz * cy * sx - sz * sy * cx,
			sz * cy * sx + cz * sy * cx,
			sz * cy * cx - cz * sy * sx));
	}

	Vector3 Quaternion::ToEuler() const
	{
		Vector3 e;
		e.x = std::atan2(2.0f * (w * x + y * z), 1.0f - 2.0f * (x * x + y * y));
		float sy = 2.0f * (w * y - z * x);
		e.y = std::fabs(sy) >= 1.0f ? std::copysign(math::kPiOver2, sy) : std::asin(sy);
		e.z = std::atan2(2.0f * (w * z + x * y), 1.0f - 2.0f * (y * y + z * z));
		return e;
	}

	// 対角成分のうち一番大きいものを基準にして、割り算の誤差を小さくする
	Quaternion Quaternion::FromMatrix(const Matrix4& a)
	{
		const auto& m = a.m;
		Quaternion q;
		float trace = m[0][0] + m[1][1] + m[2][2];
		if (trace > 0.0f)
		{
			float s = std::sqrt(trace + 1.0f) * 2.0f;
			q.w = 0.25f * s;
			q.x = (m[1][2] - m[2][1]) / s;
			q.y = (m[2][0] - m[0][2]) / s;
			q.z = (m[0][1] - m[1][0]) / s;
		}
		else if (m[0][0] > m[1][1] && m[0][0] > m[2][2])
		{
			float s = std::sqrt(1.0f + m[0][0] - m[1][1] - m[2][2]) * 2.0f;
			q.w = (m[1][2] - m[2][1]) / s;
			q.x = 0.25f * s;
			q.y = (m[0][1] + m[1][0]) / s;
			q.z = (m[2][0] + m[0][2]) / s;
		}
		else if (m[1][1] > m[2][2])
		{
			float s = std::sqrt(1.0f + m[1][1] - m[0][0] - m[2][2]) * 2.0f;
			q.w = (m[2][0] - m[0][2]) / s;
			q.x = (m[0][1] + m[1][0]) / s;
			q.y = 0.25f * s;
			q.z = (m[1][2] + m[2][1]) / s;
		}
		else
		{
			float s = std::sqrt(1.0f + m[2][2] - m[0][0] - m[1][1]) * 2.0f;
			q.w = (m[0][1] - m[1][0]) / s;
			q.x = (m[2][0] + m[0][2]) / s;
			q.y = (m[1][2] + m[2][1]) / s;
			q.z = 0.25f * s;
		}
		return Normalize(q);
	}

	Quaternion Quaternion::LookRotation(const Vector3& forward, const Vector3& up)
	{
		Vector3 z = Normalize(forward);
		if (LengthSq(z) < math::kEpsilon) { return kIdentity; }
		Vector3 x = Cross(up, z);
		if (LengthSq(x) < math::kEpsilon)
		{
			// forward と up が平行なときは別の up を使う
			x = Cross(std::fabs(z.y) < 0.99f ? Vector3::kUnitY : Vector3::kUnitX, z);
		}
		x = Normalize(x);
		Vector3 y = Cross(z, x);
		Matrix4 m(
			x.x, x.y, x.z, 0.0f,
			y.x, y.y, y.z, 0.0f,
			z.x, z.y, z.z, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f);
		return FromMatrix(m);
	}

	Quaternion Quaternion::FromTo(const Vector3& from, const Vector3& to)
	{
		Vector3 a = Normalize(from), b = Normalize(to);
		float d = Dot(a, b);
		if (d >= 1.0f - 1.0e-6f) { return kIdentity; }
		if (d <= -1.0f + 1.0e-6f)
		{
			// 真逆のときは回転軸が決まらないので、a に垂直な軸で半回転する
			Vector3 axis = Cross(Vector3::kUnitX, a);
			if (LengthSq(axis) < 1.0e-6f) { axis = Cross(Vector3::kUnitY, a); }
			return AngleAxis(Normalize(axis), math::kPi);
		}
		return AngleAxis(Normalize(Cross(a, b)), std::acos(math::Clamp(d, -1.0f, 1.0f)));
	}

	Quaternion Normalize(const Quaternion& q)
	{
		float len = std::sqrt(Dot(q, q));
		if (len < math::kEpsilon) { return Quaternion::kIdentity; }
		float inv = 1.0f / len;
		return { q.w * inv, q.x * inv, q.y * inv, q.z * inv };
	}

	Vector3 operator*(const Vector3& v, const Quaternion& q)
	{
		Vector3 u(q.x, q.y, q.z);
		Vector3 t = 2.0f * Cross(u, v);
		return v + q.w * t + Cross(u, t);
	}

	// 遠回りしないよう、内積が負なら片方の符号を反転してから補間する
	Quaternion Slerp(const Quaternion& a, const Quaternion& b, float t)
	{
		float cosOmega = Dot(a, b);
		Quaternion c = b;
		if (cosOmega < 0.0f)
		{
			c = { -b.w, -b.x, -b.y, -b.z };
			cosOmega = -cosOmega;
		}
		float k0, k1;
		if (cosOmega > 0.9995f)
		{
			k0 = 1.0f - t;
			k1 = t;
		}
		else
		{
			float sinOmega = std::sqrt(1.0f - cosOmega * cosOmega);
			float omega = std::atan2(sinOmega, cosOmega);
			k0 = std::sin((1.0f - t) * omega) / sinOmega;
			k1 = std::sin(t * omega) / sinOmega;
		}
		return Normalize(Quaternion(
			k0 * a.w + k1 * c.w, k0 * a.x + k1 * c.x, k0 * a.y + k1 * c.y, k0 * a.z + k1 * c.z));
	}

	Quaternion Nlerp(const Quaternion& a, const Quaternion& b, float t)
	{
		float sign = Dot(a, b) < 0.0f ? -1.0f : 1.0f;
		return Normalize(Quaternion(
			math::Lerp(a.w, b.w * sign, t), math::Lerp(a.x, b.x * sign, t),
			math::Lerp(a.y, b.y * sign, t), math::Lerp(a.z, b.z * sign, t)));
	}
}
