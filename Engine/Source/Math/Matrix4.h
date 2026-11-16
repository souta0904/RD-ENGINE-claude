#pragma once
#include "Math/Quaternion.h"
#include "Math/Vector3.h"

namespace rd
{
	// 4x4 行列（行ベクトル v * M、合成は先に適用するものを左に書く: S * R * T）
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
		static Matrix4 CreateAffine(const Vector3& s, const Quaternion& r, const Vector3& t);

		static Matrix4 CreateLookAt(const Vector3& eye, const Vector3& target, const Vector3& up);
		static Matrix4 CreatePerspectiveFov(float fovY, float aspect, float nearZ, float farZ);
		// 左上が原点のスクリーン座標用
		static Matrix4 CreateOrthographic(float left, float top, float right, float bottom, float nearZ, float farZ);
		static Matrix4 CreateOrthographicOffCenter(float left, float right, float bottom, float top, float nearZ, float farZ);

		Vector3 GetTranslation() const { return { m[3][0], m[3][1], m[3][2] }; }
		Vector3 GetScale() const;
		Quaternion GetRotation() const;
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

	inline Vector3 TransformPoint(const Vector3& v, const Matrix4& m)
	{
		return {
			v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0] + m.m[3][0],
			v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1] + m.m[3][1],
			v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] + m.m[3][2] };
	}
	inline Vector3 TransformVector(const Vector3& v, const Matrix4& m)
	{
		return {
			v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0],
			v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1],
			v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] };
	}
	// 同次座標で変換して w で割る
	Vector3 TransformCoord(const Vector3& v, const Matrix4& m);
	inline Vector3 operator*(const Vector3& v, const Matrix4& m) { return TransformPoint(v, m); }

	float Determinant(const Matrix4& m);
	Matrix4 Inverse(const Matrix4& m);
	Matrix4 Transpose(const Matrix4& m);
}
