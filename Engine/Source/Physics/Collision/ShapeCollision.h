#pragma once
#include "Math/Geometry/Ray.h"
#include "Physics/Collision/ContactPoint.h"
#include "Physics/Collision/WorldShape.h"
#include <vector>

namespace rd
{
	// 形同士の接触を調べる（接触点を out に足して、あれば true。法線は a → b）
	RD_API bool CollideShapes(const WorldShape& a, const WorldShape& b, std::vector<ContactPoint>& out);
	RD_API bool RaycastShape(const WorldShape& shape, const Ray& ray, float maxDist, float& outT, Vector3& outNormal);
}
