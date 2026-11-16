#pragma once
#include "Math/Matrix4.h"

namespace rd
{
	// 軸に平行なバウンディングボックス
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
		// 中心を変換し、広がりは行列の各成分の絶対値で包み直す
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
}
