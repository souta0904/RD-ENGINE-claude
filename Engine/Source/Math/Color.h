#pragma once
#include "Math/MathUtil.h"

namespace rd
{
	// RGBA カラー（リニア空間）
	struct Color
	{
		float r = 0.0f;
		float g = 0.0f;
		float b = 0.0f;
		float a = 1.0f;

		constexpr Color() = default;
		constexpr Color(float r, float g, float b, float a = 1.0f) : r(r), g(g), b(b), a(a) {}

		float* Data() { return &r; }
		const float* Data() const { return &r; }

		// HSV（各成分 0～1）から作成
		static Color FromHSV(float h, float s, float v, float a = 1.0f)
		{
			float r = v, g = v, b = v;
			if (s > 0.0f)
			{
				h = math::Wrap(h, 0.0f, 1.0f) * 6.0f;
				int i = int(h);
				float f = h - float(i);
				switch (i)
				{
				default:
				case 0: g *= 1.0f - s * (1.0f - f); b *= 1.0f - s; break;
				case 1: r *= 1.0f - s * f; b *= 1.0f - s; break;
				case 2: r *= 1.0f - s; b *= 1.0f - s * (1.0f - f); break;
				case 3: r *= 1.0f - s; g *= 1.0f - s * f; break;
				case 4: r *= 1.0f - s * (1.0f - f); g *= 1.0f - s; break;
				case 5: g *= 1.0f - s; b *= 1.0f - s * f; break;
				}
			}
			return { r, g, b, a };
		}

		static const Color kBlack;
		static const Color kWhite;
		static const Color kRed;
		static const Color kGreen;
		static const Color kBlue;
		static const Color kYellow;
		static const Color kCyan;
		static const Color kMagenta;
		static const Color kClear;
	};
	inline constexpr Color Color::kBlack{ 0.0f, 0.0f, 0.0f, 1.0f };
	inline constexpr Color Color::kWhite{ 1.0f, 1.0f, 1.0f, 1.0f };
	inline constexpr Color Color::kRed{ 1.0f, 0.0f, 0.0f, 1.0f };
	inline constexpr Color Color::kGreen{ 0.0f, 1.0f, 0.0f, 1.0f };
	inline constexpr Color Color::kBlue{ 0.0f, 0.0f, 1.0f, 1.0f };
	inline constexpr Color Color::kYellow{ 1.0f, 1.0f, 0.0f, 1.0f };
	inline constexpr Color Color::kCyan{ 0.0f, 1.0f, 1.0f, 1.0f };
	inline constexpr Color Color::kMagenta{ 1.0f, 0.0f, 1.0f, 1.0f };
	inline constexpr Color Color::kClear{ 0.0f, 0.0f, 0.0f, 0.0f };

	constexpr bool operator==(const Color& a, const Color& b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }
	constexpr Color operator+(const Color& a, const Color& b) { return { a.r + b.r, a.g + b.g, a.b + b.b, a.a + b.a }; }
	constexpr Color operator-(const Color& a, const Color& b) { return { a.r - b.r, a.g - b.g, a.b - b.b, a.a - b.a }; }
	constexpr Color operator*(const Color& a, const Color& b) { return { a.r * b.r, a.g * b.g, a.b * b.b, a.a * b.a }; }
	constexpr Color operator*(const Color& a, float s) { return { a.r * s, a.g * s, a.b * s, a.a * s }; }
	constexpr Color operator*(float s, const Color& a) { return a * s; }
	constexpr Color& operator+=(Color& a, const Color& b) { return a = a + b; }
	constexpr Color& operator*=(Color& a, float s) { return a = a * s; }
}
