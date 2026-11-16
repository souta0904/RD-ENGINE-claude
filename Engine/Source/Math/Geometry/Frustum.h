#pragma once
#include "Math/Geometry/AABB.h"
#include "Math/Geometry/Plane.h"
#include "Math/Vector4.h"
#include <array>

namespace rd
{
	// 視錐台（カリング用）
	struct Frustum
	{
		std::array<Plane, 6> planes;

		// ビュー射影行列の列の足し引きで 6 枚の平面を取り出す（行ベクトル規約、深度 0～1）
		static Frustum FromViewProj(const Matrix4& vp)
		{
			Frustum f;
			Vector4 c0(vp.m[0][0], vp.m[1][0], vp.m[2][0], vp.m[3][0]);
			Vector4 c1(vp.m[0][1], vp.m[1][1], vp.m[2][1], vp.m[3][1]);
			Vector4 c2(vp.m[0][2], vp.m[1][2], vp.m[2][2], vp.m[3][2]);
			Vector4 c3(vp.m[0][3], vp.m[1][3], vp.m[2][3], vp.m[3][3]);
			Vector4 raw[6] = { c3 + c0, c3 - c0, c3 + c1, c3 - c1, c2, c3 - c2 };
			for (int i = 0; i < 6; ++i)
			{
				Vector3 n = raw[i].XYZ();
				float len = Length(n);
				f.planes[i] = { n / len, raw[i].w / len };
			}
			return f;
		}

		// 箱を各平面の法線へ投影した半径より外側にあれば見えない
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
}
