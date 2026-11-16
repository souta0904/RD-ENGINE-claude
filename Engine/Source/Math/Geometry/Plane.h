#pragma once
#include "Math/Vector3.h"

namespace rd
{
	// 平面 dot(normal, p) + d = 0
	struct Plane
	{
		Vector3 normal = Vector3::kUnitY;
		float d = 0.0f;

		float Distance(const Vector3& p) const { return Dot(normal, p) + d; }
	};
}
