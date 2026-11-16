#pragma once
#include "Math/Vector3.h"

namespace rd
{
	class Collider;

	// レイキャストで当たったもの
	struct RaycastHit
	{
		Collider* collider = nullptr;
		Vector3 point;
		Vector3 normal;
		float distance = 0.0f;
	};
}
