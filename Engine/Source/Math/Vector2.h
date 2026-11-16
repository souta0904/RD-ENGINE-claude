#pragma once
#include "Math/MathUtil.h"

namespace rd
{
	// 2次元ベクトル（Vector3 と同じ機能をそろえる）
	struct Vector2
	{
		float x = 0.0f;
		float y = 0.0f;

		constexpr Vector2() = default;
		constexpr Vector2(float x, float y) : x(x), y(y) {}

		float* Data() { return &x; }
		const float* Data() const { return &x; }
		constexpr float operator[](int i) const { return i == 0 ? x : y; }
		float& operator[](int i) { return (&x)[i]; }

		Vector2& Normalize();
		Vector2 Normalized() const;

		static const Vector2 kZero;
		static const Vector2 kOne;
		static const Vector2 kUnitX;
		static const Vector2 kUnitY;
	};
	inline constexpr Vector2 Vector2::kZero{ 0.0f, 0.0f };
	inline constexpr Vector2 Vector2::kOne{ 1.0f, 1.0f };
	inline constexpr Vector2 Vector2::kUnitX{ 1.0f, 0.0f };
	inline constexpr Vector2 Vector2::kUnitY{ 0.0f, 1.0f };

	constexpr bool operator==(const Vector2& a, const Vector2& b) { return a.x == b.x && a.y == b.y; }
	constexpr Vector2 operator-(const Vector2& a) { return { -a.x, -a.y }; }
	constexpr Vector2 operator+(const Vector2& a, const Vector2& b) { return { a.x + b.x, a.y + b.y }; }
	constexpr Vector2 operator-(const Vector2& a, const Vector2& b) { return { a.x - b.x, a.y - b.y }; }
	constexpr Vector2 operator*(const Vector2& a, const Vector2& b) { return { a.x * b.x, a.y * b.y }; }
	constexpr Vector2 operator/(const Vector2& a, const Vector2& b) { return { a.x / b.x, a.y / b.y }; }
	constexpr Vector2 operator*(const Vector2& a, float s) { return { a.x * s, a.y * s }; }
	constexpr Vector2 operator*(float s, const Vector2& a) { return { a.x * s, a.y * s }; }
	constexpr Vector2 operator/(const Vector2& a, float s) { return { a.x / s, a.y / s }; }
	constexpr Vector2& operator+=(Vector2& a, const Vector2& b) { return a = a + b; }
	constexpr Vector2& operator-=(Vector2& a, const Vector2& b) { return a = a - b; }
	constexpr Vector2& operator*=(Vector2& a, const Vector2& b) { return a = a * b; }
	constexpr Vector2& operator/=(Vector2& a, const Vector2& b) { return a = a / b; }
	constexpr Vector2& operator*=(Vector2& a, float s) { return a = a * s; }
	constexpr Vector2& operator/=(Vector2& a, float s) { return a = a / s; }

	constexpr float Dot(const Vector2& a, const Vector2& b) { return a.x * b.x + a.y * b.y; }
	// 2D の外積は z 成分だけ（a から b へ反時計回りなら正）
	constexpr float Cross(const Vector2& a, const Vector2& b) { return a.x * b.y - a.y * b.x; }
	constexpr float LengthSq(const Vector2& a) { return Dot(a, a); }
	inline float Length(const Vector2& a) { return std::sqrt(LengthSq(a)); }
	inline float Distance(const Vector2& a, const Vector2& b) { return Length(a - b); }
	inline Vector2 Normalize(const Vector2& a)
	{
		float len = Length(a);
		return len > math::kEpsilon ? a / len : Vector2::kZero;
	}
	inline Vector2 Min(const Vector2& a, const Vector2& b) { return { std::min(a.x, b.x), std::min(a.y, b.y) }; }
	inline Vector2 Max(const Vector2& a, const Vector2& b) { return { std::max(a.x, b.x), std::max(a.y, b.y) }; }
	inline Vector2 Abs(const Vector2& a) { return { std::fabs(a.x), std::fabs(a.y) }; }
	constexpr Vector2 Reflect(const Vector2& v, const Vector2& n) { return v - 2.0f * Dot(v, n) * n; }
	constexpr Vector2 ProjectOnPlane(const Vector2& v, const Vector2& n) { return v - Dot(v, n) * n; }

	// メンバの中では Normalize がメンバを指すので、自由関数は rd:: を付けて呼ぶ
	inline Vector2& Vector2::Normalize() { return *this = rd::Normalize(*this); }
	inline Vector2 Vector2::Normalized() const { return rd::Normalize(*this); }
}
