#pragma once
#include "Math/Vector2.h"

namespace rd
{
	// 3x3 行列（2D のアフィン変換用）。考え方は Matrix4 の 2D 版
	//   アフィン変換 … 拡大・回転・移動を組み合わせた変換。2D の点は (x, y, 1) として 3x3 行列を掛ける
	//   m[行][列]。1 行目が X 軸、2 行目が Y 軸、3 行目が移動量になる
	struct Matrix3
	{
		// 初期値は単位行列（何も変えない変換）
		float m[3][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };

		static Matrix3 CreateScale(const Vector2& s);
		static Matrix3 CreateRotate(float radians);
		static Matrix3 CreateTranslate(const Vector2& t);
		// 拡大 → 回転 → 移動 をまとめた行列（CreateScale(s) * CreateRotate(r) * CreateTranslate(t) と同じ）
		static Matrix3 CreateAffine(const Vector2& s, float radians, const Vector2& t);

		static const Matrix3 kIdentity;
	};
	inline constexpr Matrix3 Matrix3::kIdentity{};

	// 合成。a * b は「a の変換をしてから b の変換をする」
	Matrix3 operator*(const Matrix3& a, const Matrix3& b);
	// 点 v を変換する（v を (x, y, 1) とみなすので移動も効く）
	Vector2 operator*(const Vector2& v, const Matrix3& m);
	// 行列式。0 なら逆行列がない（拡大率 0 などで潰れている）。負なら裏返し（鏡像）が入っている
	float Determinant(const Matrix3& m);
	// 逆行列（元に戻す変換）
	Matrix3 Inverse(const Matrix3& m);
	// 転置（行と列を入れ替える）
	Matrix3 Transpose(const Matrix3& m);
}
