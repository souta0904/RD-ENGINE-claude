#pragma once
#include "Math/MathUtil.h"

namespace rd
{
	// 2次元ベクトル（Vector3 と同じ機能をそろえる）。UI の座標・テクスチャ座標・スティックの入力などに使う
	struct Vector2
	{
		float x = 0.0f;
		float y = 0.0f;

		// constexpr を付けると、コンパイル時に値を決められる（下の kZero などの定数を作れる）
		constexpr Vector2() = default;
		constexpr Vector2(float x, float y) : x(x), y(y) {}

		// float の並びとして先頭を返す（GPU や ImGui に float 配列として渡すため）
		float* Data() { return &x; }
		const float* Data() const { return &x; }
		// v[0] で x、v[1] で y。書き込み用は x と y がメモリ上で隣に並んでいることを利用している
		constexpr float operator[](int i) const { return i == 0 ? x : y; }
		float& operator[](int i) { return (&x)[i]; }

		// Normalize は自分自身を長さ 1 にする、Normalized は長さ 1 にしたコピーを返す（Unity と同じ）
		RD_API Vector2& Normalize();
		RD_API Vector2 Normalized() const;

		// よく使う値。static const で宣言し、struct の定義が終わった後の下で値を入れる
		// （struct の中ではまだ Vector2 が完成していないので、ここでは値を書けない）
		static const Vector2 kZero;
		static const Vector2 kOne;
		static const Vector2 kUnitX;
		static const Vector2 kUnitY;
	};
	inline constexpr Vector2 Vector2::kZero{ 0.0f, 0.0f };
	inline constexpr Vector2 Vector2::kOne{ 1.0f, 1.0f };
	inline constexpr Vector2 Vector2::kUnitX{ 1.0f, 0.0f };
	inline constexpr Vector2 Vector2::kUnitY{ 0.0f, 1.0f };

	// 演算子はメンバではなく自由関数にしている。こうすると 2.0f * v と v * 2.0f の両方を書ける
	// == は完全に一致するかを調べる。計算した値どうしを比べるなら math::NearlyEqual を成分ごとに使う
	constexpr bool operator==(const Vector2& a, const Vector2& b) { return a.x == b.x && a.y == b.y; }
	constexpr Vector2 operator-(const Vector2& a) { return { -a.x, -a.y }; }
	// ベクトルどうしの * と / は成分ごとの計算（内積ではない。内積は Dot）
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

	// 内積。2 つのベクトルが同じ向きなら正、直角なら 0、反対向きなら負
	constexpr float Dot(const Vector2& a, const Vector2& b) { return a.x * b.x + a.y * b.y; }
	// 2D の外積は z 成分だけ（a から b へ反時計回りなら正）
	constexpr float Cross(const Vector2& a, const Vector2& b) { return a.x * b.y - a.y * b.x; }
	// 長さの 2 乗。sqrt を使わないので速い。長さどうしを比べるだけならこちらで足りる
	constexpr float LengthSq(const Vector2& a) { return Dot(a, a); }
	inline float Length(const Vector2& a) { return std::sqrt(LengthSq(a)); }
	inline float Distance(const Vector2& a, const Vector2& b) { return Length(a - b); }
	// 長さ 1 にする。長さがほぼ 0 のときは 0 で割らないよう kZero を返す
	inline Vector2 Normalize(const Vector2& a)
	{
		float len = Length(a);
		return len > math::kEpsilon ? a / len : Vector2::kZero;
	}
	inline Vector2 Min(const Vector2& a, const Vector2& b) { return { std::min(a.x, b.x), std::min(a.y, b.y) }; }
	inline Vector2 Max(const Vector2& a, const Vector2& b) { return { std::max(a.x, b.x), std::max(a.y, b.y) }; }
	inline Vector2 Abs(const Vector2& a) { return { std::fabs(a.x), std::fabs(a.y) }; }
	// v を法線 n の面で跳ね返した向き（n は長さ 1）。v から n 方向の成分を 2 回引くと向きが反転する
	constexpr Vector2 Reflect(const Vector2& v, const Vector2& n) { return v - 2.0f * Dot(v, n) * n; }
	// v から法線 n 方向の成分を取り除いた、面に沿った向き（n は長さ 1）。壁に沿って滑らせるときなどに使う
	constexpr Vector2 ProjectOnPlane(const Vector2& v, const Vector2& n) { return v - Dot(v, n) * n; }

	// メンバの中では Normalize がメンバを指すので、自由関数は rd:: を付けて呼ぶ
	inline Vector2& Vector2::Normalize() { return *this = rd::Normalize(*this); }
	inline Vector2 Vector2::Normalized() const { return rd::Normalize(*this); }
}
