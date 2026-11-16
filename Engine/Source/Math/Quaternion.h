#pragma once
#include "Math/Vector3.h"

namespace rd
{
	struct Matrix4;

	// 回転を表すクォータニオン
	//  a * b は「a を適用した後に b を適用する」回転（行ベクトルの行列と同じ順序）
	struct Quaternion
	{
		float w = 1.0f;
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;

		constexpr Quaternion() = default;
		constexpr Quaternion(float w, float x, float y, float z) : w(w), x(x), y(y), z(z) {}

		static Quaternion AngleAxis(const Vector3& axis, float radians);
		// X→Y→Z の順に適用するオイラー角（ラジアン）
		static Quaternion FromEuler(const Vector3& radians);
		static Quaternion FromMatrix(const Matrix4& m);
		static Quaternion LookRotation(const Vector3& forward, const Vector3& up = Vector3::kUnitY);
		static Quaternion FromTo(const Vector3& from, const Vector3& to);

		Vector3 ToEuler() const;

		static const Quaternion kIdentity;
	};
	inline constexpr Quaternion Quaternion::kIdentity{ 1.0f, 0.0f, 0.0f, 0.0f };

	constexpr bool operator==(const Quaternion& a, const Quaternion& b) { return a.w == b.w && a.x == b.x && a.y == b.y && a.z == b.z; }

	constexpr Quaternion operator*(const Quaternion& a, const Quaternion& b)
	{
		return {
			a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
			a.w * b.x + a.x * b.w + a.z * b.y - a.y * b.z,
			a.w * b.y + a.y * b.w + a.x * b.z - a.z * b.x,
			a.w * b.z + a.z * b.w + a.y * b.x - a.x * b.y };
	}
	constexpr Quaternion& operator*=(Quaternion& a, const Quaternion& b) { return a = a * b; }

	constexpr Quaternion Conjugate(const Quaternion& q) { return { q.w, -q.x, -q.y, -q.z }; }
	constexpr float Dot(const Quaternion& a, const Quaternion& b) { return a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z; }
	Quaternion Normalize(const Quaternion& q);
	inline Quaternion Inverse(const Quaternion& q) { return Conjugate(Normalize(q)); }

	Vector3 operator*(const Vector3& v, const Quaternion& q);
	inline Vector3 Rotate(const Vector3& v, const Quaternion& q) { return v * q; }

	Quaternion Slerp(const Quaternion& a, const Quaternion& b, float t);
	Quaternion Nlerp(const Quaternion& a, const Quaternion& b, float t);
}
