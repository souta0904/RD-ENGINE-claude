#pragma once
#include "Math/Geometry/AABB.h"
#include "Math/Geometry/Ray.h"
#include <functional>
#include <vector>

namespace rd
{
	// メッシュの衝突データ（メッシュのローカル座標の三角形と BVH。拡大縮小は焼き込み済み）
	//  三角形の表（外向き）は Cross(c - a, b - a) の向き（エンジンのモデルやプリミティブと同じ巻き順）
	//  動く物体のメッシュ用に、凸包として当てるための点と面も Build で作る（凹んだメッシュでもこの凸包で当たる）
	class RD_API MeshCollisionData
	{
	public:
		// 外向き。Dot(p, normal) <= d が内側
		struct Plane
		{
			Vector3 normal;
			float d;
		};

		void Build(std::vector<Vector3> positions, std::vector<uint32_t> indices);
		const std::vector<Vector3>& GetPositions() const { return mPositions; }

		const std::vector<Vector3>& GetHullPoints() const { return mHullPoints; }
		const std::vector<Plane>& GetHullPlanes() const { return mHullPlanes; }
		const AABB& GetBounds() const { return mNodes.empty() ? mEmpty : mNodes[0].bounds; }
		size_t GetTriangleCount() const { return mTriangles.size(); }
		void GetTriangle(uint32_t index, Vector3& a, Vector3& b, Vector3& c) const;
		// box と重なるかもしれない三角形の番号を順に渡す
		void Query(const AABB& box, const std::function<void(uint32_t)>& callback) const;
		bool Raycast(const Ray& ray, float maxDist, float& outT, Vector3& outNormal) const;

	private:
		// count が 0 なら内部ノード（left / right が子）、それ以外は葉（start から count 個の三角形）
		struct Node
		{
			AABB bounds;
			uint32_t left = 0;
			uint32_t right = 0;
			uint32_t start = 0;
			uint32_t count = 0;
		};
		uint32_t BuildNode(uint32_t start, uint32_t count, int depth);
		void BuildHull();
		void BuildHullPoints();
		void BuildHullPlanes();

		std::vector<Vector3> mHullPoints;
		std::vector<Plane> mHullPlanes;
		std::vector<Vector3> mPositions;
		std::vector<uint32_t> mIndices;
		std::vector<uint32_t> mTriangles;
		std::vector<AABB> mTriBounds;
		std::vector<Vector3> mTriCenters;
		std::vector<Node> mNodes;
		AABB mEmpty;
	};
}
