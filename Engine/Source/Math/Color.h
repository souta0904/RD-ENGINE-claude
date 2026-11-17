#pragma once
#include "Math/MathUtil.h"

namespace rd
{
	// RGBA カラー（リニア空間）。各成分は 0～1（HDR の光の強さなどでは 1 を超えてもよい）
	//   リニア空間 … 明るさが値に比例する色の表し方。光の計算（足す・掛ける）はこの空間で行う。
	//   画像ファイルやモニターは sRGB（暗い所に細かく値を割り当てた表し方）なので、
	//   テクスチャを読むときと画面に出すときに GPU が変換する
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

		// HSV（各成分 0～1）から作る。色相を 6 つの区間に分けて計算する
		//   h … 色相（0=赤 → 1/6=黄 → 2/6=緑 → 3/6=水色 → 4/6=青 → 5/6=紫 → 1=赤に戻る）
		//   s … 彩度（0 で白黒）、v … 明るさ（0 で黒）
		// 色相を少しずつずらして別々の色を作りたいとき（デバッグ表示の色分けなど）に便利
		static Color FromHSV(float h, float s, float v, float a = 1.0f)
		{
			// まず白黒（r=g=b=v）から始め、区間 i ごとに減らす成分を決める。f は区間の中での位置（0～1）
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
		static const Color kClear;// 透明な黒
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

	// 色どうしの * は成分ごとの掛け算（物の色 × 光の色 のように使う）
	constexpr bool operator==(const Color& a, const Color& b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }
	constexpr Color operator+(const Color& a, const Color& b) { return { a.r + b.r, a.g + b.g, a.b + b.b, a.a + b.a }; }
	constexpr Color operator-(const Color& a, const Color& b) { return { a.r - b.r, a.g - b.g, a.b - b.b, a.a - b.a }; }
	constexpr Color operator*(const Color& a, const Color& b) { return { a.r * b.r, a.g * b.g, a.b * b.b, a.a * b.a }; }
	constexpr Color operator*(const Color& a, float s) { return { a.r * s, a.g * s, a.b * s, a.a * s }; }
	constexpr Color operator*(float s, const Color& a) { return a * s; }
	constexpr Color& operator+=(Color& a, const Color& b) { return a = a + b; }
	constexpr Color& operator*=(Color& a, float s) { return a = a * s; }
}
