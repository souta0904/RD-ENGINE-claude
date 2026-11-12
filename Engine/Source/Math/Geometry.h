#pragma once
#include "Math/Matrix.h"
#include <array>

namespace rd
{
	// 軸平行バウンディングボックス
	struct AABB
	{
		Vector3 min = { math::kInfinity, math::kInfinity, math::kInfinity };
		Vector3 max = { -math::kInfinity, -math::kInfinity, -math::kInfinity };

		AABB() = default;
		AABB(const Vector3& min, const Vector3& max) : min(min), max(max) {}
		static AABB FromCenterExtents(const Vector3& c, const Vector3& e) { return { c - e, c + e }; }

		bool IsValid() const { return min.x <= max.x && min.y <= max.y && min.z <= max.z; }
		Vector3 GetCenter() const { return (min + max) * 0.5f; }
		Vector3 GetExtents() const { return (max - min) * 0.5f; }
		void Expand(const Vector3& p) { min = Min(min, p); max = Max(max, p); }
		void Expand(const AABB& b) { if (b.IsValid()) { min = Min(min, b.min); max = Max(max, b.max); } }
		bool Contains(const Vector3& p) const
		{
			return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y && p.z >= min.z && p.z <= max.z;
		}
		bool Overlaps(const AABB& b) const
		{
			return min.x <= b.max.x && max.x >= b.min.x && min.y <= b.max.y && max.y >= b.min.y && min.z <= b.max.z && max.z >= b.min.z;
		}
		// 行列で変換した結果を包む AABB
		AABB Transform(const Matrix4& m) const
		{
			if (!IsValid()) { return *this; }
			Vector3 c = TransformPoint(GetCenter(), m);
			Vector3 e = GetExtents();
			Vector3 ne(
				std::fabs(m.m[0][0]) * e.x + std::fabs(m.m[1][0]) * e.y + std::fabs(m.m[2][0]) * e.z,
				std::fabs(m.m[0][1]) * e.x + std::fabs(m.m[1][1]) * e.y + std::fabs(m.m[2][1]) * e.z,
				std::fabs(m.m[0][2]) * e.x + std::fabs(m.m[1][2]) * e.y + std::fabs(m.m[2][2]) * e.z);
			return FromCenterExtents(c, ne);
		}
	};

	struct Ray
	{
		Vector3 origin;
		Vector3 direction = Vector3::kUnitZ;// 正規化済み

		Vector3 GetPoint(float t) const { return origin + direction * t; }
	};

	struct Plane
	{
		Vector3 normal = Vector3::kUnitY;
		float d = 0.0f;// dot(normal, p) + d = 0

		float Distance(const Vector3& p) const { return Dot(normal, p) + d; }
	};

	// 視錐台（カリング用）
	struct Frustum
	{
		std::array<Plane, 6> planes;

		// ビュー射影行列から作成（行ベクトル規約、深度 0～1）
		static Frustum FromViewProj(const Matrix4& vp)
		{
			Frustum f;
			auto col = [&](int c) { return Vector4(vp.m[0][c], vp.m[1][c], vp.m[2][c], vp.m[3][c]); };
			Vector4 c0 = col(0), c1 = col(1), c2 = col(2), c3 = col(3);
			Vector4 raw[6] = { c3 + c0, c3 - c0, c3 + c1, c3 - c1, c2, c3 - c2 };
			for (int i = 0; i < 6; ++i)
			{
				Vector3 n = raw[i].XYZ();
				float len = Length(n);
				f.planes[i] = { n / len, raw[i].w / len };
			}
			return f;
		}

		bool Intersects(const AABB& box) const
		{
			if (!box.IsValid()) { return true; }
			Vector3 c = box.GetCenter();
			Vector3 e = box.GetExtents();
			for (const auto& p : planes)
			{
				float r = e.x * std::fabs(p.normal.x) + e.y * std::fabs(p.normal.y) + e.z * std::fabs(p.normal.z);
				if (p.Distance(c) < -r) { return false; }
			}
			return true;
		}
	};

	// レイと AABB の交差（スラブ法）
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

	// レイと三角形の交差（Möller–Trumbore、両面）
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
