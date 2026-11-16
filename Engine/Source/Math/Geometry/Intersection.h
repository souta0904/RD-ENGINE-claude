#pragma once
#include "Math/Geometry/AABB.h"
#include "Math/Geometry/Ray.h"

namespace rd
{
	// レイと AABB の交差（スラブ法: 軸ごとに入る距離と出る距離を求めて重なりを調べる）
	inline bool RayIntersectsAABB(const Ray& ray, const AABB& box, float& outT)
	{
		float tmin = 0.0f, tmax = math::kInfinity;
		for (int i = 0; i < 3; ++i)
		{
			float o = ray.origin[i], d = ray.direction[i];
			float lo = box.min[i], hi = box.max[i];
			if (std::fabs(d) < 1.0e-8f)
			{
				if (o < lo || o > hi) { return false; }
				continue;
			}
			float inv = 1.0f / d;
			float t1 = (lo - o) * inv, t2 = (hi - o) * inv;
			if (t1 > t2) { std::swap(t1, t2); }
			tmin = std::max(tmin, t1);
			tmax = std::min(tmax, t2);
			if (tmin > tmax) { return false; }
		}
		outT = tmin;
		return true;
	}

	// レイと三角形の交差（Möller–Trumbore 法、両面）
	inline bool RayIntersectsTriangle(const Ray& ray, const Vector3& a, const Vector3& b, const Vector3& c, float& outT)
	{
		Vector3 e1 = b - a, e2 = c - a;
		Vector3 p = Cross(ray.direction, e2);
		float det = Dot(e1, p);
		if (std::fabs(det) < 1.0e-8f) { return false; }
		float inv = 1.0f / det;
		Vector3 s = ray.origin - a;
		float u = Dot(s, p) * inv;
		if (u < 0.0f || u > 1.0f) { return false; }
		Vector3 q = Cross(s, e1);
		float v = Dot(ray.direction, q) * inv;
		if (v < 0.0f || u + v > 1.0f) { return false; }
		float t = Dot(e2, q) * inv;
		if (t < 0.0f) { return false; }
		outT = t;
		return true;
	}
}
