#pragma once
#include "Math/MathUtil.h"

namespace rd
{
	// Vector2 と Vector3 は同じ機能をそろえる（片方に足したらもう片方にも足す）
	//   演算子: == 単項- + - *(成分) /(成分) *(スカラー) /(スカラー) と各複合代入
	//   関数  : Dot Cross LengthSq Length Distance Normalize Min Max Abs Reflect ProjectOnPlane
	//   メンバ: Data operator[] Normalize（自身を正規化） Normalized（正規化したコピー）

	// ==================================================
	// 2次元ベクトル
	// ==================================================
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

		// 自身を正規化する（長さ 0 のときはゼロベクトルになる）
		Vector2& Normalize();
		// 正規化したコピーを返す（自身は変えない）
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
	// 反射ベクトル（n は正規化済み）
	constexpr Vector2 Reflect(const Vector2& v, const Vector2& n) { return v - 2.0f * Dot(v, n) * n; }
	// v を n に垂直な直線へ投影（n は正規化済み）
	constexpr Vector2 ProjectOnPlane(const Vector2& v, const Vector2& n) { return v - Dot(v, n) * n; }

	// メンバの中では Normalize がメンバを指すので、自由関数は rd:: を付けて呼ぶ
	inline Vector2& Vector2::Normalize() { return *this = rd::Normalize(*this); }
	inline Vector2 Vector2::Normalized() const { return rd::Normalize(*this); }

	// ==================================================
	// 3次元ベクトル
	// ==================================================
	struct Vector3
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;

		constexpr Vector3() = default;
		constexpr Vector3(float x, float y, float z) : x(x), y(y), z(z) {}
		constexpr explicit Vector3(const Vector2& v, float z = 0.0f) : x(v.x), y(v.y), z(z) {}

		float* Data() { return &x; }
		const float* Data() const { return &x; }
		constexpr float operator[](int i) const { return i == 0 ? x : (i == 1 ? y : z); }
		float& operator[](int i) { return (&x)[i]; }
		constexpr Vector2 XY() const { return { x, y }; }

		// 自身を正規化する（長さ 0 のときはゼロベクトルになる）
		Vector3& Normalize();
		// 正規化したコピーを返す（自身は変えない）
		Vector3 Normalized() const;

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
	// 反射ベクトル（n は正規化済み）
	constexpr Vector3 Reflect(const Vector3& v, const Vector3& n) { return v - 2.0f * Dot(v, n) * n; }
	// v を n に垂直な平面へ投影（n は正規化済み）
	constexpr Vector3 ProjectOnPlane(const Vector3& v, const Vector3& n) { return v - Dot(v, n) * n; }

	inline Vector3& Vector3::Normalize() { return *this = rd::Normalize(*this); }
	inline Vector3 Vector3::Normalized() const { return rd::Normalize(*this); }

	// ==================================================
	// 4次元ベクトル
	// ==================================================
	struct Vector4
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
		float w = 0.0f;

		constexpr Vector4() = default;
		constexpr Vector4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
		constexpr Vector4(const Vector3& v, float w) : x(v.x), y(v.y), z(v.z), w(w) {}

		float* Data() { return &x; }
		const float* Data() const { return &x; }
		constexpr Vector3 XYZ() const { return { x, y, z }; }
	};

	constexpr bool operator==(const Vector4& a, const Vector4& b) { return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w; }
	constexpr Vector4 operator+(const Vector4& a, const Vector4& b) { return { a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w }; }
	constexpr Vector4 operator-(const Vector4& a, const Vector4& b) { return { a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w }; }
	constexpr Vector4 operator*(const Vector4& a, float s) { return { a.x * s, a.y * s, a.z * s, a.w * s }; }
	constexpr Vector4 operator*(float s, const Vector4& a) { return a * s; }
	constexpr float Dot(const Vector4& a, const Vector4& b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }
}
