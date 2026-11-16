#pragma once
#include "Math/Vector3.h"

namespace rd
{
	// 接触点 1 つ（normal は A から B へ向かう向き）
	struct ContactPoint
	{
		Vector3 point;
		Vector3 normal;
		float depth = 0.0f;
	};
}
