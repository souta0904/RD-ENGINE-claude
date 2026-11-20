#pragma once
#include "Physics/Collision/WorldShape.h"

// 最近点を求める幾何の関数（Real-Time Collision Detection の 5.1 節）
namespace rd
{
	RD_API Vector3 ClosestPointOnSegment(const Vector3& p, const Vector3& a, const Vector3& b, float* outT = nullptr);
	RD_API void ClosestPointsSegmentSegment(const Vector3& p1, const Vector3& q1, const Vector3& p2, const Vector3& q2, Vector3& c1, Vector3& c2);
	RD_API Vector3 ClosestPointOnTriangle(const Vector3& p, const Vector3& a, const Vector3& b, const Vector3& c);
	RD_API Vector3 ClosestPointOnBox(const Vector3& p, const WorldShape& box);
}
