#pragma once
#include "Math/Geometry/AABB.h"
#include "Math/Geometry/Ray.h"

namespace rd
{
	// レイと AABB の交差（スラブ法: 軸ごとに入る距離と出る距離を求めて重なりを調べる）
	//   スラブ … 2 枚の平行な面に挟まれた板（例: x が min.x～max.x の範囲）。箱は X・Y・Z 3 枚のスラブの重なり。
	//   レイが各スラブの中にいる距離の範囲 [t1, t2] を求め、3 つすべてに共通する範囲があれば当たっている。
	//   当たったら、箱に入る距離（始点が箱の中なら 0）を outT に入れる
	inline bool RayIntersectsAABB(const Ray& ray, const AABB& box, float& outT)
	{
		// 共通の範囲。始点より後ろ（t < 0）は見ないので 0 から始める
		float tmin = 0.0f, tmax = math::kInfinity;
		for (int i = 0; i < 3; ++i)
		{
			float o = ray.origin[i], d = ray.direction[i];
			float lo = box.min[i], hi = box.max[i];
			// レイがこの軸に平行なら、始点がスラブの中にいるかだけで決まる（0 で割らないよう先に調べる）
			if (std::fabs(d) < 1.0e-8f)
			{
				if (o < lo || o > hi) { return false; }
				continue;
			}
			// 2 枚の面に届く距離。レイが負の向きなら hi の方が先に届くので、小さい方を t1 にする
			float inv = 1.0f / d;
			float t1 = (lo - o) * inv, t2 = (hi - o) * inv;
			if (t1 > t2) { std::swap(t1, t2); }
			tmin = std::max(tmin, t1);
			tmax = std::min(tmax, t2);
			// 共通の範囲がなくなったら外れ
			if (tmin > tmax) { return false; }
		}
		outT = tmin;
		return true;
	}

	// レイと三角形の交差（Möller–Trumbore 法、両面）
	//   三角形の中の点は a + u(b - a) + v(c - a)（u >= 0、v >= 0、u + v <= 1）と書ける。
	//   これがレイの点 origin + t * direction と等しくなる t・u・v を、外積と内積だけで求める。
	//   「両面」… 裏から当たっても当たりにする（det の符号を見ていない）
	inline bool RayIntersectsTriangle(const Ray& ray, const Vector3& a, const Vector3& b, const Vector3& c, float& outT)
	{
		// 三角形の 2 辺
		Vector3 e1 = b - a, e2 = c - a;
		Vector3 p = Cross(ray.direction, e2);
		float det = Dot(e1, p);
		// det が 0 に近いのは、レイが三角形の面と平行なとき（当たらない）
		if (std::fabs(det) < 1.0e-8f) { return false; }
		float inv = 1.0f / det;
		Vector3 s = ray.origin - a;
		// u・v が範囲外なら、面には当たるが三角形の外
		float u = Dot(s, p) * inv;
		if (u < 0.0f || u > 1.0f) { return false; }
		Vector3 q = Cross(s, e1);
		float v = Dot(ray.direction, q) * inv;
		if (v < 0.0f || u + v > 1.0f) { return false; }
		// t < 0 は始点より後ろで当たっている
		float t = Dot(e2, q) * inv;
		if (t < 0.0f) { return false; }
		outT = t;
		return true;
	}
}
