#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

// 数学でよく使う定数と小さな関数をまとめたもの（Unity の Mathf に当たる）
//
// 座標系: 左手座標系、行ベクトル（v * M）、合成は先に適用するものを左に書く（S * R * T）
//   左手座標系 … X が右、Y が上、Z が画面の奥（DirectX と同じ。Unity も同じ）
//   行ベクトル … ベクトルを横に並べて、行列の左から掛ける（v * M）
//   合成の順番 … 「拡大してから回転して移動する」なら S * R * T と、読む順に左から書ける
namespace rd::math
{
	inline constexpr float kPi = std::numbers::pi_v<float>;
	inline constexpr float k2Pi = kPi * 2.0f;
	inline constexpr float kPiOver2 = kPi / 2.0f;
	inline constexpr float kInfinity = std::numeric_limits<float>::infinity();
	// これより小さい値は 0 とみなす（長さ 0 のベクトルを正規化しないためなどに使う）
	inline constexpr float kEpsilon = 1.0e-6f;

	// 度 ⇔ ラジアン。エンジンの関数はすべてラジアンを受け取り、エディタの表示だけ度にする
	inline constexpr float ToRadians(float degrees) { return degrees * (kPi / 180.0f); }
	inline constexpr float ToDegrees(float radians) { return radians * (180.0f / kPi); }

	// value を lo～hi の範囲に収める
	template <class T>
	constexpr T Clamp(const T& value, const T& lo, const T& hi) { return std::min(hi, std::max(lo, value)); }

	// 0～1 に収める（HLSL の saturate と同じ名前）
	inline constexpr float Saturate(float value) { return Clamp(value, 0.0f, 1.0f); }

	// a と b の間を t で直線的に補間する（t=0 で a、t=1 で b）。t は範囲外でもそのまま延長する
	// + と * があれば使えるので、float・ベクトル・色のどれにも使える
	template <class T>
	constexpr T Lerp(const T& a, const T& b, float t) { return a + (b - a) * t; }

	// a～b の中で value がどの位置か（0～1 に収める）。Lerp の逆
	inline constexpr float InverseLerp(float a, float b, float value) { return a != b ? Saturate((value - a) / (b - a)) : 0.0f; }

	// 始めと終わりがゆっくりな補間（Unity の Mathf.SmoothStep と同じ）
	// t を 3t^2 - 2t^3 の曲線に通してから Lerp する。この曲線は t=0 と t=1 で傾きが 0 になる
	inline constexpr float SmoothStep(float from, float to, float t)
	{
		t = Saturate(t);
		t = t * t * (3.0f - 2.0f * t);
		return Lerp(from, to, t);
	}

	// current を target へ最大 maxDelta だけ近づける（行き過ぎない）。一定の速さで近づけたいときに使う
	inline float MoveTowards(float current, float target, float maxDelta)
	{
		if (std::fabs(target - current) <= maxDelta) { return target; }
		return current + (target > current ? maxDelta : -maxDelta);
	}

	// 正なら 1、負なら -1、0 なら 0（比較の結果 true/false を 1/0 として引き算している）
	inline constexpr int Sign(float value) { return (value > 0.0f) - (value < 0.0f); }

	// float は計算の誤差で == がほぼ成り立たないので、差が eps 以下なら等しいとみなす
	inline bool NearlyEqual(float a, float b, float eps = 1.0e-4f) { return std::fabs(a - b) <= eps; }

	// フレームレートに関係なく同じ速さで目標へ近づける（カメラの追従などに使う）
	//   毎フレーム Lerp(current, target, 0.1f) とすると、60fps と 30fps で近づく速さが変わってしまう。
	//   1 - exp(-smoothing * dt) を使うと、dt を 2 回に分けて呼んでも 1 回でまとめて呼んでも同じ結果になる。
	//   smoothing が大きいほど速く近づく（目安: 1 秒で約 1 - exp(-smoothing) の割合だけ近づく）
	inline float Damp(float current, float target, float smoothing, float dt)
	{
		return Lerp(current, target, 1.0f - std::exp(-smoothing * dt));
	}

	// value を min～max の範囲に繰り返しで収める（はみ出した分は反対側から入る）
	// 例: Wrap(370, 0, 360) = 10、Wrap(-10, 0, 360) = 350。角度を 0～360 に戻すときなどに使う
	inline float Wrap(float value, float min, float max)
	{
		float range = max - min;
		return value - range * std::floor((value - min) / range);
	}
}
