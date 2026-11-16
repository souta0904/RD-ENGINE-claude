#pragma once
#include "Math/Vector3.h"

namespace rd
{
	// 半直線（direction は正規化済み）
	struct Ray
	{
		Vector3 origin;
		Vector3 direction = Vector3::kUnitZ;

		Vector3 GetPoint(float t) const { return origin + direction * t; }
	};
}
