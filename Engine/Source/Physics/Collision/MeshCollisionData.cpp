#include "RdPch.h"
#include "Physics/Collision/MeshCollisionData.h"
#include "Math/Geometry/Intersection.h"

namespace rd
{
	namespace
	{
		constexpr int kStackSize = 96;

		struct PlaneCandidate
		{
			Vector3 normal;
			float area;
		};

		bool LargerArea(const PlaneCandidate& a, const PlaneCandidate& b) { return a.area > b.area; }

		bool ContainsPoint(const std::vector<Vector3>& points, const Vector3& p)
		{
			for (const Vector3& q : points)
			{
				if (LengthSq(p - q) < 1.0e-8f) { return true; }
			}
			return false;
		}
	}

	void MeshCollisionData::Build(std::vector<Vector3> positions, std::vector<uint32_t> indices)
	{
		mPositions = std::move(positions);
		mIndices = std::move(indices);
		uint32_t triCount = uint32_t(mIndices.size() / 3);
		mTriangles.resize(triCount);
		mTriBounds.resize(triCount);
		mTriCenters.resize(triCount);
		for (uint32_t i = 0; i < triCount; ++i)
		{
			Vector3 a, b, c;
			GetTriangle(i, a, b, c);
			mTriangles[i] = i;
			mTriBounds[i] = AABB(Min(a, Min(b, c)), Max(a, Max(b, c)));
			mTriCenters[i] = (a + b + c) / 3.0f;
		}
		mNodes.clear();
		mNodes.reserve(size_t(triCount) / 2 + 1);
		if (triCount > 0) { BuildNode(0, triCount, 0); }
		BuildHull();
	}

	void MeshCollisionData::BuildHull()
	{
		mHullPoints.clear();
		mHullPlanes.clear();
		if (mPositions.empty()) { return; }
		BuildHullPoints();
		BuildHullPlanes();
	}

	// 同じ位置の頂点をまとめる（UV や法線の違いで分かれている頂点が多い）
	//  多すぎるときは、球面に均等に並べた方向（黄金角で回す）ごとに一番外側の点だけを残す（凸包の近似）
	void MeshCollisionData::BuildHullPoints()
	{
		constexpr size_t kMaxPoints = 256;
		constexpr size_t kGiveUp = 4096;
		std::vector<Vector3> unique;
		for (const Vector3& p : mPositions)
		{
			if (!ContainsPoint(unique, p)) { unique.push_back(p); }
			if (unique.size() > kGiveUp) { break; }
		}
		if (unique.size() <= kMaxPoints && unique.size() < kGiveUp)
		{
			mHullPoints = std::move(unique);
			return;
		}
		constexpr size_t kDirections = 162;
		for (size_t i = 0; i < kDirections; ++i)
		{
			float y = 1.0f - 2.0f * (float(i) + 0.5f) / float(kDirections);
			float r = std::sqrt(std::max(0.0f, 1.0f - y * y));
			float phi = float(i) * 2.39996323f;
			Vector3 dir(std::cos(phi) * r, y, std::sin(phi) * r);
			const Vector3* best = &mPositions[0];
			for (const Vector3& p : mPositions) { if (Dot(p, dir) > Dot(*best, dir)) { best = &p; } }
			if (!ContainsPoint(mHullPoints, *best)) { mHullPoints.push_back(*best); }
		}
	}

	// 面の向き（外向き）を集める。同じ向きはまとめ、面積の大きいものから使う
	void MeshCollisionData::BuildHullPlanes()
	{
		constexpr size_t kMaxPlanes = 64;
		std::vector<PlaneCandidate> candidates;
		for (size_t t = 0; t + 2 < mIndices.size(); t += 3)
		{
			Vector3 a = mPositions[mIndices[t]], b = mPositions[mIndices[t + 1]], c = mPositions[mIndices[t + 2]];
			Vector3 n = Cross(c - a, b - a);
			float len = Length(n);
			if (len < 1.0e-10f) { continue; }
			n /= len;
			bool merged = false;
			for (auto& cand : candidates)
			{
				if (Dot(cand.normal, n) > 0.999f) { cand.area += len; merged = true; break; }
			}
			if (!merged) { candidates.push_back({ n, len }); }
		}
		std::sort(candidates.begin(), candidates.end(), LargerArea);
		if (candidates.size() > kMaxPlanes) { candidates.resize(kMaxPlanes); }
		for (auto& cand : candidates)
		{
			float d = -math::kInfinity;
			for (const Vector3& p : mHullPoints) { d = std::max(d, Dot(p, cand.normal)); }
			mHullPlanes.push_back({ cand.normal, d });
		}
	}

	void MeshCollisionData::GetTriangle(uint32_t index, Vector3& a, Vector3& b, Vector3& c) const
	{
		a = mPositions[mIndices[size_t(index) * 3 + 0]];
		b = mPositions[mIndices[size_t(index) * 3 + 1]];
		c = mPositions[mIndices[size_t(index) * 3 + 2]];
	}

	// 三角形の中心の広がりが一番大きい軸で、真ん中の位置で 2 つに分ける
	uint32_t MeshCollisionData::BuildNode(uint32_t start, uint32_t count, int depth)
	{
		uint32_t index = uint32_t(mNodes.size());
		mNodes.emplace_back();
		AABB bounds, centers;
		for (uint32_t i = start; i < start + count; ++i)
		{
			bounds.Expand(mTriBounds[mTriangles[i]]);
			centers.Expand(mTriCenters[mTriangles[i]]);
		}
		mNodes[index].bounds = bounds;

		if (count <= 4 || depth > 32)
		{
			mNodes[index].start = start;
			mNodes[index].count = count;
			return index;
		}
		Vector3 extent = centers.max - centers.min;
		int axis = extent.x > extent.y ? (extent.x > extent.z ? 0 : 2) : (extent.y > extent.z ? 1 : 2);
		uint32_t mid = start + count / 2;
		std::nth_element(mTriangles.begin() + start, mTriangles.begin() + mid, mTriangles.begin() + start + count,
			[&](uint32_t a, uint32_t b) { return mTriCenters[a][axis] < mTriCenters[b][axis]; });

		uint32_t left = BuildNode(start, mid - start, depth + 1);
		uint32_t right = BuildNode(mid, start + count - mid, depth + 1);
		mNodes[index].left = left;
		mNodes[index].right = right;
		return index;
	}

	// 再帰の代わりに固定長のスタックでノードを辿る
	void MeshCollisionData::Query(const AABB& box, const std::function<void(uint32_t)>& callback) const
	{
		if (mNodes.empty()) { return; }
		uint32_t stack[kStackSize];
		int sp = 0;
		stack[sp++] = 0;
		while (sp > 0)
		{
			const Node& n = mNodes[stack[--sp]];
			if (!n.bounds.Overlaps(box)) { continue; }
			if (n.count > 0)
			{
				for (uint32_t i = n.start; i < n.start + n.count; ++i)
				{
					if (mTriBounds[mTriangles[i]].Overlaps(box)) { callback(mTriangles[i]); }
				}
			}
			else if (sp + 2 <= kStackSize)
			{
				stack[sp++] = n.left;
				stack[sp++] = n.right;
			}
		}
	}

	bool MeshCollisionData::Raycast(const Ray& ray, float maxDist, float& outT, Vector3& outNormal) const
	{
		if (mNodes.empty()) { return false; }
		bool hit = false;
		float best = maxDist;
		uint32_t stack[kStackSize];
		int sp = 0;
		stack[sp++] = 0;
		while (sp > 0)
		{
			const Node& n = mNodes[stack[--sp]];
			float tBox;
			if (!RayIntersectsAABB(ray, n.bounds, tBox) || tBox > best) { continue; }
			if (n.count > 0)
			{
				for (uint32_t i = n.start; i < n.start + n.count; ++i)
				{
					Vector3 a, b, c;
					GetTriangle(mTriangles[i], a, b, c);
					float t;
					if (RayIntersectsTriangle(ray, a, b, c, t) && t < best)
					{
						best = t;
						Vector3 nrm = Normalize(Cross(b - a, c - a));
						outNormal = Dot(nrm, ray.direction) > 0.0f ? -nrm : nrm;
						hit = true;
					}
				}
			}
			else if (sp + 2 <= kStackSize)
			{
				stack[sp++] = n.left;
				stack[sp++] = n.right;
			}
		}
		if (hit) { outT = best; }
		return hit;
	}
}
