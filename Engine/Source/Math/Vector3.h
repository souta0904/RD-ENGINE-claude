#pragma once
#include "Math/Vector2.h"

namespace rd
{
	// 3次元ベクトル（Vector2 と同じ機能をそろえる）。位置・向き・拡大率などに使う
	// 各関数の意味は Vector2.h のコメントと同じ
	struct Vector3
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;

		constexpr Vector3() = default;
		constexpr Vector3(float x, float y, float z) : x(x), y(y), z(z) {}
		// explicit を付けて、Vector2 が知らないうちに Vector3 へ変換されないようにする
		constexpr explicit Vector3(const Vector2& v, float z = 0.0f) : x(v.x), y(v.y), z(z) {}

		float* Data() { return &x; }
		const float* Data() const { return &x; }
		constexpr float operator[](int i) const { return i == 0 ? x : (i == 1 ? y : z); }
		float& operator[](int i) { return (&x)[i]; }
		constexpr Vector2 XY() const { return { x, y }; }

		RD_API Vector3& Normalize();
		RD_API Vector3 Normalized() const;

		static const Vector3 kZero;
		static const Vector3 kOne;
		static const Vector3 kUnitX;
		static const Vector3 kUnitY;
		static const Vector3 kUnitZ;
	};
	inline constexpr Vector3 Vector3::kZero{ 0.0f, 0.0f, 0.0f };
	inline constexpr Vector3 Vector3::kOne{ 1.0f, 1.0f, 1.0f };
	inline constexpr Vector3 Vector3::kUnitX{ 1.0f, 0.0f, 0.0f };
	inline constexpr Vector3 Vector3::kUnitY{ 0.0f, 1.0f, 0.0f };
	inline constexpr Vector3 Vector3::kUnitZ{ 0.0f, 0.0f, 1.0f };

	constexpr bool operator==(const Vector3& a, const Vector3& b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
	constexpr Vector3 operator-(const Vector3& a) { return { -a.x, -a.y, -a.z }; }
	constexpr Vector3 operator+(const Vector3& a, const Vector3& b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
	constexpr Vector3 operator-(const Vector3& a, const Vector3& b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
	constexpr Vector3 operator*(const Vector3& a, const Vector3& b) { return { a.x * b.x, a.y * b.y, a.z * b.z }; }
	constexpr Vector3 operator/(const Vector3& a, const Vector3& b) { return { a.x / b.x, a.y / b.y, a.z / b.z }; }
	constexpr Vector3 operator*(const Vector3& a, float s) { return { a.x * s, a.y * s, a.z * s }; }
	constexpr Vector3 operator*(float s, const Vector3& a) { return { a.x * s, a.y * s, a.z * s }; }
	constexpr Vector3 operator/(const Vector3& a, float s) { return { a.x / s, a.y / s, a.z / s }; }
	constexpr Vector3& operator+=(Vector3& a, const Vector3& b) { return a = a + b; }
	constexpr Vector3& operator-=(Vector3& a, const Vector3& b) { return a = a - b; }
	constexpr Vector3& operator*=(Vector3& a, const Vector3& b) { return a = a * b; }
	constexpr Vector3& operator/=(Vector3& a, const Vector3& b) { return a = a / b; }
	constexpr Vector3& operator*=(Vector3& a, float s) { return a = a * s; }
	constexpr Vector3& operator/=(Vector3& a, float s) { return a = a / s; }

	constexpr float Dot(const Vector3& a, const Vector3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
	// 外積。a と b の両方に垂直なベクトルを返す（長さは a と b が作る平行四辺形の面積）
	// 左手座標系なので Cross(右, 上) = 奥（Cross(kUnitX, kUnitY) = kUnitZ）
	constexpr Vector3 Cross(const Vector3& a, const Vector3& b)
	{
		return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
	}
	constexpr float LengthSq(const Vector3& a) { return Dot(a, a); }
	inline float Length(const Vector3& a) { return std::sqrt(LengthSq(a)); }
	inline float Distance(const Vector3& a, const Vector3& b) { return Length(a - b); }
	inline Vector3 Normalize(const Vector3& a)
	{
		float len = Length(a);
		return len > math::kEpsilon ? a / len : Vector3::kZero;
	}
	inline Vector3 Min(const Vector3& a, const Vector3& b) { return { std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z) }; }
	inline Vector3 Max(const Vector3& a, const Vector3& b) { return { std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z) }; }
	inline Vector3 Abs(const Vector3& a) { return { std::fabs(a.x), std::fabs(a.y), std::fabs(a.z) }; }
	constexpr Vector3 Reflect(const Vector3& v, const Vector3& n) { return v - 2.0f * Dot(v, n) * n; }
	constexpr Vector3 ProjectOnPlane(const Vector3& v, const Vector3& n) { return v - Dot(v, n) * n; }

	// メンバの中では Normalize がメンバを指すので、自由関数は rd:: を付けて呼ぶ
	inline Vector3& Vector3::Normalize() { return *this = rd::Normalize(*this); }
	inline Vector3 Vector3::Normalized() const { return rd::Normalize(*this); }
}
