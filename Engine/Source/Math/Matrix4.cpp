#include "RdPch.h"
#include "Math/Matrix4.h"

namespace rd
{
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
		// 鏡像（負のスケール）は X 軸に押し付ける
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

	// 上 2 行と下 2 行の 2x2 小行列式（s, c）を組み合わせて求める（ラプラス展開）
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

	// Determinant と同じ 2x2 小行列式から余因子を作って割る
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
}
