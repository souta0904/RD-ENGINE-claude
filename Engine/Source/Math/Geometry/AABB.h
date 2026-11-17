#pragma once
#include "Math/Matrix4.h"

namespace rd
{
	// 軸に平行なバウンディングボックス（Axis-Aligned Bounding Box）
	// 回転しない箱で物体を包んだもの。重なりの判定が軽いので、カリングや物理の大まかな判定に使う
	struct AABB
	{
		// 初期値は「空の箱」（min が +∞、max が -∞）。Expand で点を足していくと、最初の点でちょうどその点の箱になる
		Vector3 min = { math::kInfinity, math::kInfinity, math::kInfinity };
		Vector3 max = { -math::kInfinity, -math::kInfinity, -math::kInfinity };

		AABB() = default;
		AABB(const Vector3& min, const Vector3& max) : min(min), max(max) {}
		// 中心 c と、中心から各面までの距離 e（extents、大きさの半分）から作る
		static AABB FromCenterExtents(const Vector3& c, const Vector3& e) { return { c - e, c + e }; }

		// 空の箱（まだ何も Expand していない）なら false
		bool IsValid() const { return min.x <= max.x && min.y <= max.y && min.z <= max.z; }
		Vector3 GetCenter() const { return (min + max) * 0.5f; }
		Vector3 GetExtents() const { return (max - min) * 0.5f; }
		// 点 p（または箱 b）が入るように広げる。メッシュの全頂点を Expand すると、そのメッシュを包む箱になる
		void Expand(const Vector3& p) { min = Min(min, p); max = Max(max, p); }
		void Expand(const AABB& b) { if (b.IsValid()) { min = Min(min, b.min); max = Max(max, b.max); } }
		bool Contains(const Vector3& p) const
		{
			return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y && p.z >= min.z && p.z <= max.z;
		}
		// 2 つの箱が重なっているか。X・Y・Z のすべての軸で範囲が重なっていれば重なっている
		bool Overlaps(const AABB& b) const
		{
			return min.x <= b.max.x && max.x >= b.min.x && min.y <= b.max.y && max.y >= b.min.y && min.z <= b.max.z && max.z >= b.min.z;
		}
		// 中心を変換し、広がりは行列の各成分の絶対値で包み直す
		//   箱を回転させると軸に平行でなくなるので、回転後の箱をさらに包む AABB を返す（少し大きくなる）。
		//   8 つの角をすべて変換して Expand するのと同じ結果を、少ない計算で求められる
		//   （各軸の広がり = 回転後の 3 本の軸がその軸方向にどれだけ伸びるか、の合計）
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
