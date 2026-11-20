#pragma once
#include "Math/Quaternion.h"
#include "Math/Vector3.h"

namespace rd
{
	// 4x4 行列（行ベクトル v * M、合成は先に適用するものを左に書く: S * R * T）
	//   3D の点を (x, y, z, 1) として掛けると、拡大・回転・移動・透視投影をすべて行列 1 つで表せる
	//   m[行][列]。アフィン変換では 1～3 行目が変換後の X・Y・Z 軸、4 行目（m[3][0..2]）が移動量になる
	//
	// 使い道:
	//   ワールド行列 … モデルの座標をワールドの座標へ（Transform が作る）
	//   ビュー行列   … ワールドの座標をカメラから見た座標へ（CreateLookAt）
	//   射影行列     … カメラから見た座標を画面の座標へ（CreatePerspectiveFov / CreateOrthographic）
	//   頂点は「ワールド * ビュー * 射影」の順に掛けて画面に出す
	struct Matrix4
	{
		// 初期値は単位行列（何も変えない変換）
		float m[4][4] = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 } };

		constexpr Matrix4() = default;
		constexpr Matrix4(
			float m00, float m01, float m02, float m03,
			float m10, float m11, float m12, float m13,
			float m20, float m21, float m22, float m23,
			float m30, float m31, float m32, float m33)
			: m{ { m00, m01, m02, m03 }, { m10, m11, m12, m13 }, { m20, m21, m22, m23 }, { m30, m31, m32, m33 } }
		{}

		float* Data() { return &m[0][0]; }
		const float* Data() const { return &m[0][0]; }

		RD_API static Matrix4 CreateScale(const Vector3& s);
		static Matrix4 CreateScale(float s) { return CreateScale(Vector3(s, s, s)); }
		RD_API static Matrix4 CreateRotateX(float radians);
		RD_API static Matrix4 CreateRotateY(float radians);
		RD_API static Matrix4 CreateRotateZ(float radians);
		RD_API static Matrix4 CreateRotate(const Quaternion& q);
		RD_API static Matrix4 CreateTranslate(const Vector3& t);
		// 拡大 → 回転 → 移動 をまとめた行列（CreateScale(s) * CreateRotate(r) * CreateTranslate(t) と同じ）
		RD_API static Matrix4 CreateAffine(const Vector3& s, const Quaternion& r, const Vector3& t);

		// ビュー行列。eye（カメラの位置）から target を見る。up はカメラの大まかな上
		RD_API static Matrix4 CreateLookAt(const Vector3& eye, const Vector3& target, const Vector3& up);
		// 透視投影（遠くの物ほど小さく見える）。fovY は縦の視野角（ラジアン）、aspect は幅 / 高さ
		// 見える範囲は奥行き nearZ～farZ で、深度は 0（near）～1（far）になる（DirectX の決まり）
		RD_API static Matrix4 CreatePerspectiveFov(float fovY, float aspect, float nearZ, float farZ);
		// 平行投影（遠くても同じ大きさ）。左上が原点のスクリーン座標用（UI やスプライトに使う）
		RD_API static Matrix4 CreateOrthographic(float left, float top, float right, float bottom, float nearZ, float farZ);
		// 平行投影。範囲の中心がずれていてもよい版（影のカメラなどに使う）
		RD_API static Matrix4 CreateOrthographicOffCenter(float left, float right, float bottom, float top, float nearZ, float farZ);

		// 行列から移動・拡大・回転を取り出す
		Vector3 GetTranslation() const { return { m[3][0], m[3][1], m[3][2] }; }
		RD_API Vector3 GetScale() const;
		RD_API Quaternion GetRotation() const;
		// 3 つまとめて取り出す（ばらばらに呼ぶより無駄が少ない）
		RD_API void Decompose(Vector3& scale, Quaternion& rotation, Vector3& translation) const;

		// 変換後の X・Y・Z 軸（右・上・前の向き。拡大率の分だけ長さが変わっている）
		Vector3 GetAxisX() const { return { m[0][0], m[0][1], m[0][2] }; }
		Vector3 GetAxisY() const { return { m[1][0], m[1][1], m[1][2] }; }
		Vector3 GetAxisZ() const { return { m[2][0], m[2][1], m[2][2] }; }

		static const Matrix4 kIdentity;
	};
	inline constexpr Matrix4 Matrix4::kIdentity{};

	// 合成。a * b は「a の変換をしてから b の変換をする」
	// 結果の i 行 j 列 = a の i 行目と b の j 列目の内積。毎フレーム大量に呼ぶのでヘッダに inline で書く
	inline Matrix4 operator*(const Matrix4& a, const Matrix4& b)
	{
		Matrix4 r;
		for (int i = 0; i < 4; ++i)
		{
			for (int j = 0; j < 4; ++j)
			{
				r.m[i][j] = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j] + a.m[i][2] * b.m[2][j] + a.m[i][3] * b.m[3][j];
			}
		}
		return r;
	}
	inline Matrix4& operator*=(Matrix4& a, const Matrix4& b) { return a = a * b; }

	// 点を変換する。(x, y, z, 1) として掛けるので移動が効く（位置に使う）
	inline Vector3 TransformPoint(const Vector3& v, const Matrix4& m)
	{
		return {
			v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0] + m.m[3][0],
			v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1] + m.m[3][1],
			v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] + m.m[3][2] };
	}
	// 向きを変換する。(x, y, z, 0) として掛けるので移動は効かない（速度や向きに使う）
	inline Vector3 TransformVector(const Vector3& v, const Matrix4& m)
	{
		return {
			v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0],
			v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1],
			v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] };
	}
	// 同次座標で変換して w で割る
	// 射影行列を掛けた後に使う（透視投影は w に奥行きが入り、w で割ると遠くの物が小さくなる）
	RD_API Vector3 TransformCoord(const Vector3& v, const Matrix4& m);
	// v * m は点の変換（TransformPoint）
	inline Vector3 operator*(const Vector3& v, const Matrix4& m) { return TransformPoint(v, m); }

	// 行列式。0 なら逆行列がない。負なら裏返し（鏡像）が入っている
	RD_API float Determinant(const Matrix4& m);
	// 逆行列（元に戻す変換）。例: ワールド行列の逆でワールド座標をモデルの座標に戻す
	RD_API Matrix4 Inverse(const Matrix4& m);
	// 転置（行と列を入れ替える）。HLSL 側の行列の並びに合わせるときなどに使う
	RD_API Matrix4 Transpose(const Matrix4& m);
}
