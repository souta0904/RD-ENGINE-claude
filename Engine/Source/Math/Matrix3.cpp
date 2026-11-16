#include "RdPch.h"
#include "Math/Matrix3.h"

namespace rd
{
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
		return
			m.m[0][0] * (m.m[1][1] * m.m[2][2] - m.m[1][2] * m.m[2][1]) -
			m.m[0][1] * (m.m[1][0] * m.m[2][2] - m.m[1][2] * m.m[2][0]) +
			m.m[0][2] * (m.m[1][0] * m.m[2][1] - m.m[1][1] * m.m[2][0]);
	}

	// 余因子行列を行列式で割る
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
}
