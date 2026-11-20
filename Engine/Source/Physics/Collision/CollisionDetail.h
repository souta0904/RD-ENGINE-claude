#pragma once
#include "Physics/Collision/ContactPoint.h"
#include "Physics/Collision/WorldShape.h"
#include <vector>

// 形ごとの接触判定（ShapeCollision.cpp から使う。法線はいつも A → B）
//  PrimitiveCollision.cpp: 球・箱・カプセル同士と、箱と三角形
//  MeshCollision.cpp     : 三角形の面として扱うメッシュ
//  ConvexCollision.cpp   : 凸包として扱うメッシュ
namespace rd::collision
{
	RD_API bool SphereSphere(const Vector3& a, float ra, const Vector3& b, float rb, std::vector<ContactPoint>& out);
	// 法線は球 → 箱
	RD_API bool SphereBox(const Vector3& c, float r, const WorldShape& box, std::vector<ContactPoint>& out);
	RD_API bool BoxBox(const WorldShape& a, const WorldShape& b, std::vector<ContactPoint>& out);
	// 三角形は表側からだけ押し返す。法線は箱 → 三角形
	RD_API bool BoxTriangle(const WorldShape& box, const Vector3& a, const Vector3& b, const Vector3& c, std::vector<ContactPoint>& out);
	RD_API void ClosestSegmentBox(const Vector3& p0, const Vector3& p1, const WorldShape& box, Vector3& onSeg, Vector3& onBox);
	RD_API void ClosestSegmentTriangle(const Vector3& p0, const Vector3& p1, const Vector3& a, const Vector3& b, const Vector3& c,
		Vector3& onSeg, Vector3& onTri);
	RD_API void BoxVertices(const WorldShape& box, Vector3 out[8]);
	RD_API float ProjectBox(const WorldShape& box, const Vector3& axis);
	// 接触点が多すぎると不安定になるので、before 以降を深いものから max 個に絞る
	RD_API void LimitContacts(std::vector<ContactPoint>& out, size_t before, size_t max);

	RD_API bool CollideWithMeshShape(const WorldShape& s, const WorldShape& meshShape, std::vector<ContactPoint>& out);
	RD_API bool MeshMesh(const WorldShape& a, const WorldShape& b, std::vector<ContactPoint>& out);

	RD_API bool BoxConvexMesh(const WorldShape& box, const WorldShape& mesh, std::vector<ContactPoint>& out);
	RD_API bool ConvexMeshMesh(const WorldShape& a, const WorldShape& b, std::vector<ContactPoint>& out);
	RD_API bool SphereConvex(const WorldShape& sphere, const WorldShape& mesh, std::vector<ContactPoint>& out);
	RD_API bool CapsuleConvex(const WorldShape& capsule, const WorldShape& mesh, std::vector<ContactPoint>& out);
}
