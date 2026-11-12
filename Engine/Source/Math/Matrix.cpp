#include "RdPch.h"
#include "Math/Matrix.h"

namespace rd
{
	// ==================================================
	// Matrix3
	// ==================================================
	Matrix3 Matrix3::CreateScale(const Vector2& s)
	{
		Matrix3 r;
		r.m[0][0] = s.x;
		r.m[1][1] = s.y;
		return r;
	}

	Matrix3 Matrix3::CreateRotate(float radians)
	{
		float c = std::cos(radians), s = std::sin(radians);
		Matrix3 r;
		r.m[0][0] = c; r.m[0][1] = s;
		r.m[1][0] = -s; r.m[1][1] = c;
		return r;
	}

	Matrix3 Matrix3::CreateTranslate(const Vector2& t)
	{
		Matrix3 r;
		r.m[2][0] = t.x;
		r.m[2][1] = t.y;
		return r;
	}

	Matrix3 Matrix3::CreateAffine(const Vector2& s, float radians, const Vector2& t)
	{
		Matrix3 r = CreateRotate(radians);
		r.m[0][0] *= s.x; r.m[0][1] *= s.x;
		r.m[1][0] *= s.y; r.m[1][1] *= s.y;
		r.m[2][0] = t.x; r.m[2][1] = t.y;
		return r;
	}

	Matrix3 operator*(const Matrix3& a, const Matrix3& b)
	{
		Matrix3 r;
		for (int i = 0; i < 3; ++i)
		{
			for (int j = 0; j < 3; ++j)
			{
				r.m[i][j] = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j] + a.m[i][2] * b.m[2][j];
			}
		}
		return r;
	}

	Vector2 operator*(const Vector2& v, const Matrix3& m)
	{
		return { v.x * m.m[0][0] + v.y * m.m[1][0] + m.m[2][0], v.x * m.m[0][1] + v.y * m.m[1][1] + m.m[2][1] };
	}

	float Determinant(const Matrix3& m)
	{
		// 旧実装は余因子の積が引き算になっていたので修正
		return
			m.m[0][0] * (m.m[1][1] * m.m[2][2] - m.m[1][2] * m.m[2][1]) -
			m.m[0][1] * (m.m[1][0] * m.m[2][2] - m.m[1][2] * m.m[2][0]) +
			m.m[0][2] * (m.m[1][0] * m.m[2][1] - m.m[1][1] * m.m[2][0]);
	}

	Matrix3 Inverse(const Matrix3& m)
	{
		float d = Determinant(m);
		RD_ASSERT(std::fabs(d) > 1.0e-12f, "Matrix3 is singular");
		float inv = 1.0f / d;
		Matrix3 r;
		r.m[0][0] = (m.m[1][1] * m.m[2][2] - m.m[1][2] * m.m[2][1]) * inv;
		r.m[0][1] = (m.m[0][2] * m.m[2][1] - m.m[0][1] * m.m[2][2]) * inv;
		r.m[0][2] = (m.m[0][1] * m.m[1][2] - m.m[0][2] * m.m[1][1]) * inv;
		r.m[1][0] = (m.m[1][2] * m.m[2][0] - m.m[1][0] * m.m[2][2]) * inv;
		r.m[1][1] = (m.m[0][0] * m.m[2][2] - m.m[0][2] * m.m[2][0]) * inv;
		r.m[1][2] = (m.m[0][2] * m.m[1][0] - m.m[0][0] * m.m[1][2]) * inv;
		r.m[2][0] = (m.m[1][0] * m.m[2][1] - m.m[1][1] * m.m[2][0]) * inv;
		r.m[2][1] = (m.m[0][1] * m.m[2][0] - m.m[0][0] * m.m[2][1]) * inv;
		r.m[2][2] = (m.m[0][0] * m.m[1][1] - m.m[0][1] * m.m[1][0]) * inv;
		return r;
	}

	Matrix3 Transpose(const Matrix3& m)
	{
		Matrix3 r;
		for (int i = 0; i < 3; ++i)
		{
			for (int j = 0; j < 3; ++j) { r.m[i][j] = m.m[j][i]; }
		}
		return r;
	}

	// ==================================================
	// Matrix4
	// ==================================================
	Matrix4 Matrix4::CreateScale(const Vector3& s)
	{
		Matrix4 r;
		r.m[0][0] = s.x;
		r.m[1][1] = s.y;
		r.m[2][2] = s.z;
		return r;
	}

	Matrix4 Matrix4::CreateRotateX(float radians)
	{
		float c = std::cos(radians), s = std::sin(radians);
		Matrix4 r;
		r.m[1][1] = c; r.m[1][2] = s;
		r.m[2][1] = -s; r.m[2][2] = c;
		return r;
	}

	Matrix4 Matrix4::CreateRotateY(float radians)
	{
		float c = std::cos(radians), s = std::sin(radians);
		Matrix4 r;
		r.m[0][0] = c; r.m[0][2] = -s;
		r.m[2][0] = s; r.m[2][2] = c;
		return r;
	}

	Matrix4 Matrix4::CreateRotateZ(float radians)
	{
		float c = std::cos(radians), s = std::sin(radians);
		Matrix4 r;
		r.m[0][0] = c; r.m[0][1] = s;
		r.m[1][0] = -s; r.m[1][1] = c;
		return r;
	}

	Matrix4 Matrix4::CreateRotate(const Quaternion& q)
	{
		float ww = 2.0f * q.w, xx = 2.0f * q.x, yy = 2.0f * q.y, zz = 2.0f * q.z;
		Matrix4 r;
		r.m[0][0] = 1.0f - yy * q.y - zz * q.z;
		r.m[0][1] = xx * q.y + ww * q.z;
		r.m[0][2] = xx * q.z - ww * q.y;
		r.m[1][0] = xx * q.y - ww * q.z;
		r.m[1][1] = 1.0f - xx * q.x - zz * q.z;
		r.m[1][2] = yy * q.z + ww * q.x;
		r.m[2][0] = xx * q.z + ww * q.y;
		r.m[2][1] = yy * q.z - ww * q.x;
		r.m[2][2] = 1.0f - xx * q.x - yy * q.y;
		return r;
	}

	Matrix4 Matrix4::CreateTranslate(const Vector3& t)
	{
		Matrix4 r;
		r.m[3][0] = t.x;
		r.m[3][1] = t.y;
		r.m[3][2] = t.z;
		return r;
	}

	Matrix4 Matrix4::CreateAffine(const Vector3& s, const Quaternion& q, const Vector3& t)
	{
		Matrix4 r = CreateRotate(q);
		for (int j = 0; j < 3; ++j)
		{
			r.m[0][j] *= s.x;
			r.m[1][j] *= s.y;
			r.m[2][j] *= s.z;
		}
		r.m[3][0] = t.x;
		r.m[3][1] = t.y;
		r.m[3][2] = t.z;
		return r;
	}

	Matrix4 Matrix4::CreateLookAt(const Vector3& eye, const Vector3& target, const Vector3& up)
	{
		Vector3 z = Normalize(target - eye);
		Vector3 x = Normalize(Cross(up, z));
		Vector3 y = Cross(z, x);
		return {
			x.x, y.x, z.x, 0.0f,
			x.y, y.y, z.y, 0.0f,
			x.z, y.z, z.z, 0.0f,
			-Dot(x, eye), -Dot(y, eye), -Dot(z, eye), 1.0f };
	}

	Matrix4 Matrix4::CreatePerspectiveFov(float fovY, float aspect, float nearZ, float farZ)
	{
		float yScale = 1.0f / std::tan(fovY * 0.5f);
		float xScale = yScale / aspect;
		float range = farZ / (farZ - nearZ);
		return {
			xScale, 0.0f, 0.0f, 0.0f,
			0.0f, yScale, 0.0f, 0.0f,
			0.0f, 0.0f, range, 1.0f,
			0.0f, 0.0f, -nearZ * range, 0.0f };
	}

	Matrix4 Matrix4::CreateOrthographic(float left, float top, float right, float bottom, float nearZ, float farZ)
	{
		return {
			2.0f / (right - left), 0.0f, 0.0f, 0.0f,
			0.0f, 2.0f / (top - bottom), 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f / (farZ - nearZ), 0.0f,
			(left + right) / (left - right), (top + bottom) / (bottom - top), nearZ / (nearZ - farZ), 1.0f };
	}

	Matrix4 Matrix4::CreateOrthographicOffCenter(float left, float right, float bottom, float top, float nearZ, float farZ)
	{
		float range = 1.0f / (farZ - nearZ);
		return {
			2.0f / (right - left), 0.0f, 0.0f, 0.0f,
			0.0f, 2.0f / (top - bottom), 0.0f, 0.0f,
			0.0f, 0.0f, range, 0.0f,
			(left + right) / (left - right), (top + bottom) / (bottom - top), -nearZ * range, 1.0f };
	}

	Vector3 Matrix4::GetScale() const
	{
		return { Length(GetAxisX()), Length(GetAxisY()), Length(GetAxisZ()) };
	}

	Quaternion Matrix4::GetRotation() const
	{
		Vector3 s;
		Quaternion r;
		Vector3 t;
		Decompose(s, r, t);
		return r;
	}

	void Matrix4::Decompose(Vector3& scale, Quaternion& rotation, Vector3& translation) const
	{
		translation = GetTranslation();
		scale = GetScale();
		Matrix4 rot;
		Vector3 axes[3] = { GetAxisX(), GetAxisY(), GetAxisZ() };
		for (int i = 0; i < 3; ++i)
		{
			float s = scale[i] > math::kEpsilon ? 1.0f / scale[i] : 0.0f;
			rot.m[i][0] = axes[i].x * s;
			rot.m[i][1] = axes[i].y * s;
			rot.m[i][2] = axes[i].z * s;
		}
		// 鏡像（負のスケール）を X 軸に押し付ける
		if (Determinant(rot) < 0.0f)
		{
			scale.x = -scale.x;
			for (int j = 0; j < 3; ++j) { rot.m[0][j] = -rot.m[0][j]; }
		}
		rotation = Quaternion::FromMatrix(rot);
	}

	Vector3 TransformCoord(const Vector3& v, const Matrix4& m)
	{
		Vector3 p = TransformPoint(v, m);
		float w = v.x * m.m[0][3] + v.y * m.m[1][3] + v.z * m.m[2][3] + m.m[3][3];
		return std::fabs(w) > math::kEpsilon ? p / w : p;
	}

	float Determinant(const Matrix4& a)
	{
		const auto& m = a.m;
		float s0 = m[0][0] * m[1][1] - m[1][0] * m[0][1];
		float s1 = m[0][0] * m[1][2] - m[1][0] * m[0][2];
		float s2 = m[0][0] * m[1][3] - m[1][0] * m[0][3];
		float s3 = m[0][1] * m[1][2] - m[1][1] * m[0][2];
		float s4 = m[0][1] * m[1][3] - m[1][1] * m[0][3];
		float s5 = m[0][2] * m[1][3] - m[1][2] * m[0][3];
		float c5 = m[2][2] * m[3][3] - m[3][2] * m[2][3];
		float c4 = m[2][1] * m[3][3] - m[3][1] * m[2][3];
		float c3 = m[2][1] * m[3][2] - m[3][1] * m[2][2];
		float c2 = m[2][0] * m[3][3] - m[3][0] * m[2][3];
		float c1 = m[2][0] * m[3][2] - m[3][0] * m[2][2];
		float c0 = m[2][0] * m[3][1] - m[3][0] * m[2][1];
		return s0 * c5 - s1 * c4 + s2 * c3 + s3 * c2 - s4 * c1 + s5 * c0;
	}

	Matrix4 Inverse(const Matrix4& a)
	{
		const auto& m = a.m;
		float s0 = m[0][0] * m[1][1] - m[1][0] * m[0][1];
		float s1 = m[0][0] * m[1][2] - m[1][0] * m[0][2];
		float s2 = m[0][0] * m[1][3] - m[1][0] * m[0][3];
		float s3 = m[0][1] * m[1][2] - m[1][1] * m[0][2];
		float s4 = m[0][1] * m[1][3] - m[1][1] * m[0][3];
		float s5 = m[0][2] * m[1][3] - m[1][2] * m[0][3];
		float c5 = m[2][2] * m[3][3] - m[3][2] * m[2][3];
		float c4 = m[2][1] * m[3][3] - m[3][1] * m[2][3];
		float c3 = m[2][1] * m[3][2] - m[3][1] * m[2][2];
		float c2 = m[2][0] * m[3][3] - m[3][0] * m[2][3];
		float c1 = m[2][0] * m[3][2] - m[3][0] * m[2][2];
		float c0 = m[2][0] * m[3][1] - m[3][0] * m[2][1];
		float det = s0 * c5 - s1 * c4 + s2 * c3 + s3 * c2 - s4 * c1 + s5 * c0;
		RD_ASSERT(std::fabs(det) > 1.0e-12f, "Matrix4 is singular");
		float inv = 1.0f / det;

		Matrix4 r;
		r.m[0][0] = (m[1][1] * c5 - m[1][2] * c4 + m[1][3] * c3) * inv;
		r.m[0][1] = (-m[0][1] * c5 + m[0][2] * c4 - m[0][3] * c3) * inv;
		r.m[0][2] = (m[3][1] * s5 - m[3][2] * s4 + m[3][3] * s3) * inv;
		r.m[0][3] = (-m[2][1] * s5 + m[2][2] * s4 - m[2][3] * s3) * inv;
		r.m[1][0] = (-m[1][0] * c5 + m[1][2] * c2 - m[1][3] * c1) * inv;
		r.m[1][1] = (m[0][0] * c5 - m[0][2] * c2 + m[0][3] * c1) * inv;
		r.m[1][2] = (-m[3][0] * s5 + m[3][2] * s2 - m[3][3] * s1) * inv;
		r.m[1][3] = (m[2][0] * s5 - m[2][2] * s2 + m[2][3] * s1) * inv;
		r.m[2][0] = (m[1][0] * c4 - m[1][1] * c2 + m[1][3] * c0) * inv;
		r.m[2][1] = (-m[0][0] * c4 + m[0][1] * c2 - m[0][3] * c0) * inv;
		r.m[2][2] = (m[3][0] * s4 - m[3][1] * s2 + m[3][3] * s0) * inv;
		r.m[2][3] = (-m[2][0] * s4 + m[2][1] * s2 - m[2][3] * s0) * inv;
		r.m[3][0] = (-m[1][0] * c3 + m[1][1] * c1 - m[1][2] * c0) * inv;
		r.m[3][1] = (m[0][0] * c3 - m[0][1] * c1 + m[0][2] * c0) * inv;
		r.m[3][2] = (-m[3][0] * s3 + m[3][1] * s1 - m[3][2] * s0) * inv;
		r.m[3][3] = (m[2][0] * s3 - m[2][1] * s1 + m[2][2] * s0) * inv;
		return r;
	}

	Matrix4 Transpose(const Matrix4& m)
	{
		Matrix4 r;
		for (int i = 0; i < 4; ++i)
		{
			for (int j = 0; j < 4; ++j) { r.m[i][j] = m.m[j][i]; }
		}
		return r;
	}

	// ==================================================
	// Quaternion
	// ==================================================
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
			// forward と up が平行な場合は別の up を使う
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
