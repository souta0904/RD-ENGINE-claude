#pragma once
#include "Math/Vector3.h"

namespace rd
{
	// 平面 dot(normal, p) + d = 0
	//   normal … 面の向き（長さ 1）。d … 原点から面までの距離にマイナスを付けたもの
	//   例: 高さ 2 の水平な床は normal = (0, 1, 0)、d = -2
	struct Plane
	{
		Vector3 normal = Vector3::kUnitY;
		float d = 0.0f;

		// 点 p から面までの符号付きの距離。法線の向いている側なら正、裏側なら負
		float Distance(const Vector3& p) const { return Dot(normal, p) + d; }
	};
}
