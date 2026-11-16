#pragma once
#include "Math/Vector2.h"

namespace rd
{
	// 3x3 行列（2D のアフィン変換用）
	struct Matrix3
	{
		float m[3][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };

		static Matrix3 CreateScale(const Vector2& s);
		static Matrix3 CreateRotate(float radians);
		static Matrix3 CreateTranslate(const Vector2& t);
		static Matrix3 CreateAffine(const Vector2& s, float radians, const Vector2& t);

		static const Matrix3 kIdentity;
	};
	inline constexpr Matrix3 Matrix3::kIdentity{};

	Matrix3 operator*(const Matrix3& a, const Matrix3& b);
	Vector2 operator*(const Vector2& v, const Matrix3& m);
	float Determinant(const Matrix3& m);
	Matrix3 Inverse(const Matrix3& m);
	Matrix3 Transpose(const Matrix3& m);
}
