#pragma once
#include "Math/Vector3.h"

namespace rd
{
	struct Matrix4;

	// 回転を表すクォータニオン
	//  a * b は「a を適用した後に b を適用する」回転（行ベクトルの行列と同じ順序）
	//  ※ 数学の教科書（ハミルトン積）とは順番が逆。行列の S * R * T と同じ向きに読めるようにしている
	//
	// 軸 n のまわりに角度 θ 回す回転は (w, x, y, z) = (cos(θ/2), n * sin(θ/2)) で表す。
	// オイラー角と違ってジンバルロック（2 つの軸が重なって回せなくなる）が起きず、
	// 2 つの回転の間をなめらかに補間できる（Slerp）ので、Transform の回転はこれで持つ
	struct Quaternion
	{
		// w が実部、x・y・z が虚部。初期値 (1, 0, 0, 0) は「回転なし」
		float w = 1.0f;
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;

		constexpr Quaternion() = default;
		constexpr Quaternion(float w, float x, float y, float z) : w(w), x(x), y(y), z(z) {}

		// 軸 axis のまわりに radians だけ回す
		RD_API static Quaternion AngleAxis(const Vector3& axis, float radians);
		// X→Y→Z の順に適用するオイラー角（ラジアン）
		RD_API static Quaternion FromEuler(const Vector3& radians);
		// 回転行列から作る（拡大の入っていない行列を渡す）
		RD_API static Quaternion FromMatrix(const Matrix4& m);
		// Z 軸（前）が forward を向き、Y 軸（上）ができるだけ up に近くなる回転（Unity と同じ）
		RD_API static Quaternion LookRotation(const Vector3& forward, const Vector3& up = Vector3::kUnitY);
		// 向き from を向き to に重ねる、一番小さな回転
		RD_API static Quaternion FromTo(const Vector3& from, const Vector3& to);

		// オイラー角（ラジアン、X→Y→Z の順）に戻す。エディタのインスペクタで角度を表示するときに使う
		// Y が ±90 度に近いと X と Z が区別できなくなるので、FromEuler に戻しても同じ値になるとは限らない
		RD_API Vector3 ToEuler() const;

		static const Quaternion kIdentity;
	};
	inline constexpr Quaternion Quaternion::kIdentity{ 1.0f, 0.0f, 0.0f, 0.0f };

	constexpr bool operator==(const Quaternion& a, const Quaternion& b) { return a.w == b.w && a.x == b.x && a.y == b.y && a.z == b.z; }

	// 回転の合成。a * b は「a で回してから b で回す」（上の説明のとおり教科書とは逆順）
	constexpr Quaternion operator*(const Quaternion& a, const Quaternion& b)
	{
		return {
			a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
			a.w * b.x + a.x * b.w + a.z * b.y - a.y * b.z,
			a.w * b.y + a.y * b.w + a.x * b.z - a.z * b.x,
			a.w * b.z + a.z * b.w + a.y * b.x - a.x * b.y };
	}
	constexpr Quaternion& operator*=(Quaternion& a, const Quaternion& b) { return a = a * b; }

	// 共役（虚部の符号を反転）。長さ 1 のクォータニオンなら、これが逆回転になる
	constexpr Quaternion Conjugate(const Quaternion& q) { return { q.w, -q.x, -q.y, -q.z }; }
	// 内積。2 つの回転がどれだけ近いかの目安（1 か -1 に近いほど同じ回転）
	constexpr float Dot(const Quaternion& a, const Quaternion& b) { return a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z; }
	// 長さ 1 にする。回転を何度も掛け合わせると誤差で長さがずれるので、ときどき直す
	RD_API Quaternion Normalize(const Quaternion& q);
	// 逆回転
	inline Quaternion Inverse(const Quaternion& q) { return Conjugate(Normalize(q)); }

	// ベクトル v を q で回す（行列と同じく v * q と書く）
	RD_API Vector3 operator*(const Vector3& v, const Quaternion& q);
	inline Vector3 Rotate(const Vector3& v, const Quaternion& q) { return v * q; }

	// 2 つの回転の間を補間する（t=0 で a、t=1 で b）
	//   Slerp … 回る速さが一定になる（球面線形補間）。カメラの回転など見た目の速さが大事なときに使う
	//   Nlerp … 4 成分を Lerp して正規化するだけ。速さは少し一定でないが計算が軽い。アニメーションのブレンドなどに使う
	RD_API Quaternion Slerp(const Quaternion& a, const Quaternion& b, float t);
	RD_API Quaternion Nlerp(const Quaternion& a, const Quaternion& b, float t);
}
