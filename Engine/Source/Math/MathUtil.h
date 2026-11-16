#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

// 座標系: 左手座標系、行ベクトル（v * M）、合成は先に適用するものを左に書く（S * R * T）
namespace rd::math
{
	inline constexpr float kPi = std::numbers::pi_v<float>;
	inline constexpr float k2Pi = kPi * 2.0f;
	inline constexpr float kPiOver2 = kPi / 2.0f;
	inline constexpr float kInfinity = std::numeric_limits<float>::infinity();
	inline constexpr float kEpsilon = 1.0e-6f;

	inline constexpr float ToRadians(float degrees) { return degrees * (kPi / 180.0f); }
	inline constexpr float ToDegrees(float radians) { return radians * (180.0f / kPi); }

	template <class T>
	constexpr T Clamp(const T& value, const T& lo, const T& hi) { return std::min(hi, std::max(lo, value)); }

	inline constexpr float Saturate(float value) { return Clamp(value, 0.0f, 1.0f); }

	template <class T>
	constexpr T Lerp(const T& a, const T& b, float t) { return a + (b - a) * t; }

	inline constexpr int Sign(float value) { return (value > 0.0f) - (value < 0.0f); }

	inline bool NearlyEqual(float a, float b, float eps = 1.0e-4f) { return std::fabs(a - b) <= eps; }

	// フレームレートに関係なく同じ速さで目標へ近づける
	inline float Damp(float current, float target, float smoothing, float dt)
	{
		return Lerp(current, target, 1.0f - std::exp(-smoothing * dt));
	}

	inline float Wrap(float value, float min, float max)
	{
		float range = max - min;
		return value - range * std::floor((value - min) / range);
	}
}
