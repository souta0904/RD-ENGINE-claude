#pragma once
#include "Math/Geometry.h"
#include <functional>
#include <vector>

namespace rd
{
	enum class ShapeType : uint8_t { Sphere, Box, Capsule, Mesh };

	// 静的メッシュの衝突データ（ワールド座標の三角形 + BVH）
	class MeshCollisionData
	{
	public:
		void Build(std::vector<Vector3> positions, std::vector<uint32_t> indices);
		const AABB& GetBounds() const { return mNodes.empty() ? mEmpty : mNodes[0].bounds; }
		size_t GetTriangleCount() const { return mTriangles.size(); }
		void GetTriangle(uint32_t index, Vector3& a, Vector3& b, Vector3& c) const;
		// AABB と重なる可能性のある三角形を列挙
		void Query(const AABB& box, const std::function<void(uint32_t)>& callback) const;
		bool Raycast(const Ray& ray, float maxDist, float& outT, Vector3& outNormal) const;

	private:
		struct Node
		{
			AABB bounds;
			uint32_t left = 0;// 内部ノードの子
			uint32_t right = 0;
			uint32_t start = 0;// 葉の三角形範囲
			uint32_t count = 0;// 0 なら内部ノード
		};
		uint32_t BuildNode(uint32_t start, uint32_t count, int depth);

		std::vector<Vector3> mPositions;
		std::vector<uint32_t> mIndices;
		std::vector<uint32_t> mTriangles;// BVH 順に並べた三角形番号
		std::vector<AABB> mTriBounds;
		std::vector<Vector3> mTriCenters;
		std::vector<Node> mNodes;
		AABB mEmpty;
	};

	// ワールド空間での形状
	struct WorldShape
	{
		ShapeType type = ShapeType::Sphere;
		Vector3 center;
		Vector3 axes[3] = { Vector3::kUnitX, Vector3::kUnitY, Vector3::kUnitZ };// Box の軸
		Vector3 halfExtents;// Box
		float radius = 0.5f;// Sphere / Capsule
		Vector3 p0, p1;// Capsule の線分
		const MeshCollisionData* mesh = nullptr;
		AABB bounds;

		void ComputeBounds();
	};

	struct ContactPoint
	{
		Vector3 point;
		Vector3 normal;// A から B へ向かう向き
		float depth = 0.0f;
	};

	// 形状同士の接触を求める（接触点を out に追加して、あれば true）
	bool CollideShapes(const WorldShape& a, const WorldShape& b, std::vector<ContactPoint>& out);
	// レイと形状の交差
	bool RaycastShape(const WorldShape& shape, const Ray& ray, float maxDist, float& outT, Vector3& outNormal);

	// 幾何ユーティリティ
	Vector3 ClosestPointOnSegment(const Vector3& p, const Vector3& a, const Vector3& b, float* outT = nullptr);
	void ClosestPointsSegmentSegment(const Vector3& p1, const Vector3& q1, const Vector3& p2, const Vector3& q2, Vector3& c1, Vector3& c2);
	Vector3 ClosestPointOnTriangle(const Vector3& p, const Vector3& a, const Vector3& b, const Vector3& c);
	Vector3 ClosestPointOnBox(const Vector3& p, const WorldShape& box);
}
