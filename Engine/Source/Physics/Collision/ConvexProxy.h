#pragma once
#include "Physics/Collision/WorldShape.h"
#include <vector>

namespace rd
{
	// 凸な形（箱・凸包として扱うメッシュ）を、頂点と外向きの面の集まりとして表したもの
	class RD_API ConvexProxy
	{
	public:
		static ConvexProxy FromBox(const WorldShape& box);
		static ConvexProxy FromMesh(const WorldShape& mesh);

		bool Contains(const Vector3& p, float tolerance) const;
		// 押し返す向き n に対して横の面だけで調べる（薄い板を突き抜けた点も接触として拾うため）
		bool ContainsSideways(const Vector3& p, const Vector3& n, float tolerance) const;
		float Radius() const;
		void Project(const Vector3& axis, float& mn, float& mx) const;

		std::vector<Vector3> points;
		std::vector<MeshCollisionData::Plane> planes;
	};
}
