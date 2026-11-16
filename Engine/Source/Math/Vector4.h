#pragma once
#include "Math/Vector3.h"

namespace rd
{
	// 4次元ベクトル（同次座標・シェーダーへ渡す値）
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
