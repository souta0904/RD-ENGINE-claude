#pragma once
#include "Math/Quaternion.h"
#include "Math/Vector.h"

namespace rd
{
	// ==================================================
	// 3x3 行列（2D アフィン変換用）
	// ==================================================
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
	Vector2 operator*(const Vector2& v, const Matrix3& m);// 点として変換
	float Determinant(const Matrix3& m);
	Matrix3 Inverse(const Matrix3& m);
	Matrix3 Transpose(const Matrix3& m);

	// ==================================================
	// 4x4 行列
	// ==================================================
	struct Matrix4
	{
		float m[4][4] = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 } };

		constexpr Matrix4() = default;
		constexpr Matrix4(
			float m00, float m01, float m02, float m03,
			float m10, float m11, float m12, float m13,
			float m20, float m21, float m22, float m23,
			float m30, float m31, float m32, float m33)
			: m{ { m00, m01, m02, m03 }, { m10, m11, m12, m13 }, { m20, m21, m22, m23 }, { m30, m31, m32, m33 } }
		{}

		float* Data() { return &m[0][0]; }
		const float* Data() const { return &m[0][0]; }

		static Matrix4 CreateScale(const Vector3& s);
		static Matrix4 CreateScale(float s) { return CreateScale(Vector3(s, s, s)); }
		static Matrix4 CreateRotateX(float radians);
		static Matrix4 CreateRotateY(float radians);
		static Matrix4 CreateRotateZ(float radians);
		static Matrix4 CreateRotate(const Quaternion& q);
		static Matrix4 CreateTranslate(const Vector3& t);
		// S * R * T
		static Matrix4 CreateAffine(const Vector3& s, const Quaternion& r, const Vector3& t);

		static Matrix4 CreateLookAt(const Vector3& eye, const Vector3& target, const Vector3& up);
		static Matrix4 CreatePerspectiveFov(float fovY, float aspect, float nearZ, float farZ);
		// スクリーン座標系（左上原点）用の正射影
		static Matrix4 CreateOrthographic(float left, float top, float right, float bottom, float nearZ, float farZ);
		// 標準的な正射影（中心が原点）
		static Matrix4 CreateOrthographicOffCenter(float left, float right, float bottom, float top, float nearZ, float farZ);

		Vector3 GetTranslation() const { return { m[3][0], m[3][1], m[3][2] }; }
		Vector3 GetScale() const;
		Quaternion GetRotation() const;
		// 行列を S/R/T へ分解
		void Decompose(Vector3& scale, Quaternion& rotation, Vector3& translation) const;

		Vector3 GetAxisX() const { return { m[0][0], m[0][1], m[0][2] }; }
		Vector3 GetAxisY() const { return { m[1][0], m[1][1], m[1][2] }; }
		Vector3 GetAxisZ() const { return { m[2][0], m[2][1], m[2][2] }; }

		static const Matrix4 kIdentity;
	};
	inline constexpr Matrix4 Matrix4::kIdentity{};

	inline Matrix4 operator*(const Matrix4& a, const Matrix4& b)
	{
		Matrix4 r;
		for (int i = 0; i < 4; ++i)
		{
			for (int j = 0; j < 4; ++j)
			{
				r.m[i][j] = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j] + a.m[i][2] * b.m[2][j] + a.m[i][3] * b.m[3][j];
			}
		}
		return r;
	}
	inline Matrix4& operator*=(Matrix4& a, const Matrix4& b) { return a = a * b; }

	// 点として変換（w=1）
	inline Vector3 TransformPoint(const Vector3& v, const Matrix4& m)
	{
		return {
			v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0] + m.m[3][0],
			v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1] + m.m[3][1],
			v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] + m.m[3][2] };
	}
	// 方向として変換（w=0）
	inline Vector3 TransformVector(const Vector3& v, const Matrix4& m)
	{
		return {
			v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0],
			v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1],
			v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] };
	}
	// 同次座標で変換し w で除算
	Vector3 TransformCoord(const Vector3& v, const Matrix4& m);
	inline Vector3 operator*(const Vector3& v, const Matrix4& m) { return TransformPoint(v, m); }

	float Determinant(const Matrix4& m);
	Matrix4 Inverse(const Matrix4& m);// 一般の 4x4 逆行列
	Matrix4 Transpose(const Matrix4& m);
}
