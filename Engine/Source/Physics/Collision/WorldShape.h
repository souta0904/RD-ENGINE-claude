#pragma once
#include "Math/Geometry/AABB.h"
#include "Physics/Collision/MeshCollisionData.h"

namespace rd
{
	enum class ShapeType : uint8_t { Sphere, Box, Capsule, Mesh };

	// ワールド空間での当たりの形
	//  Box: center / axes / halfExtents、Sphere: center / radius、Capsule: 線分 p0-p1 と radius
	//  Mesh: ローカルの三角形と、ローカル → ワールドの回転と移動（拡大縮小は mesh に焼き込む）
	//        meshConvex なら凸包として扱い（動く物体）、そうでなければ三角形の面として扱う（地形など）
	struct RD_API WorldShape
	{
		ShapeType type = ShapeType::Sphere;
		Vector3 center;
		Vector3 axes[3] = { Vector3::kUnitX, Vector3::kUnitY, Vector3::kUnitZ };
		Vector3 halfExtents;
		float radius = 0.5f;
		Vector3 p0, p1;
		const MeshCollisionData* mesh = nullptr;
		Quaternion meshRotation;
		Vector3 meshPosition;
		bool meshConvex = false;
		AABB bounds;

		void ComputeBounds();
		Vector3 MeshToWorld(const Vector3& p) const { return p * meshRotation + meshPosition; }
		Vector3 WorldToMesh(const Vector3& p) const { return (p - meshPosition) * Conjugate(meshRotation); }
	};
}
