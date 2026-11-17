#pragma once
#include "Math/Vector3.h"

namespace rd
{
	// 半直線（direction は正規化済み）。origin から direction の向きに無限に伸びる
	// マウスで物を選ぶとき（カメラから画面の奥へ飛ばす）や、物理のレイキャストに使う
	struct Ray
	{
		Vector3 origin;
		Vector3 direction = Vector3::kUnitZ;

		// 始点から距離 t の位置。交差判定の関数は当たった位置をこの t で返す
		Vector3 GetPoint(float t) const { return origin + direction * t; }
	};
}
