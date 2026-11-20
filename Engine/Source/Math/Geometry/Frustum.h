#pragma once
#include "Math/Geometry/AABB.h"
#include "Math/Geometry/Plane.h"
#include "Math/Vector4.h"
#include <array>

namespace rd
{
	// 視錐台（カリング用）
	//   カメラに写る範囲を表す、先を切った四角すいの形。左・右・下・上・手前・奥の 6 枚の平面で囲まれる。
	//   この範囲に入らない物は描いても写らないので、描画の前に省く（視錐台カリング）
	struct RD_API Frustum
	{
		// 0=左 1=右 2=下 3=上 4=手前 5=奥。法線はすべて内側（見える側）を向く
		std::array<Plane, 6> planes;

		// ビュー射影行列の列の足し引きで 6 枚の平面を取り出す（行ベクトル規約、深度 0～1）
		//   点 p をビュー射影行列で変換した (x, y, z, w) が写るのは -w <= x <= w、-w <= y <= w、0 <= z <= w のとき。
		//   例えば x >= -w は「p と (列3 + 列0) の内積 >= 0」と同じなので、列3 + 列0 がそのまま左の平面の式になる
		//   （Gribb と Hartmann の方法）
		static Frustum FromViewProj(const Matrix4& vp)
		{
			Frustum f;
			Vector4 c0(vp.m[0][0], vp.m[1][0], vp.m[2][0], vp.m[3][0]);
			Vector4 c1(vp.m[0][1], vp.m[1][1], vp.m[2][1], vp.m[3][1]);
			Vector4 c2(vp.m[0][2], vp.m[1][2], vp.m[2][2], vp.m[3][2]);
			Vector4 c3(vp.m[0][3], vp.m[1][3], vp.m[2][3], vp.m[3][3]);
			Vector4 raw[6] = { c3 + c0, c3 - c0, c3 + c1, c3 - c1, c2, c3 - c2 };
			// 法線の長さが 1 になるように、平面の式全体を法線の長さで割る（Distance を本当の距離にするため）
			for (int i = 0; i < 6; ++i)
			{
				Vector3 n = raw[i].XYZ();
				float len = Length(n);
				f.planes[i] = { n / len, raw[i].w / len };
			}
			return f;
		}

		// 箱を各平面の法線へ投影した半径より外側にあれば見えない
		//   r … 箱を平面の法線の向きから見たときの「中心から一番遠い角までの距離」。
		//   中心が平面の外側に r より離れていれば、箱全体が外にあるので見えない。
		//   1 枚でも外側なら false。すべて通れば true（実際には見えない箱を true にすることもあるが、描きすぎるだけで困らない）
		bool Intersects(const AABB& box) const
		{
			// 空の箱は大きさが分からないので、念のため見えるものとして扱う
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
