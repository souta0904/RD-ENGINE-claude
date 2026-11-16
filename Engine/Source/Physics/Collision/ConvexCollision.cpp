#include "RdPch.h"
#include "Physics/Collision/CollisionDetail.h"
#include "Physics/Collision/ConvexProxy.h"

namespace rd::collision
{
	namespace
	{
		struct SatResult
		{
			float overlap = math::kInfinity;
			Vector3 normal;
		};

		// 両方の点をこの軸に投影する。B が +axis 側にあるときの重なりが o1、-axis 側が o2
		bool TestConvexAxis(const ConvexProxy& a, const ConvexProxy& b, const Vector3& axis, SatResult& best)
		{
			float minA, maxA, minB, maxB;
			a.Project(axis, minA, maxA);
			b.Project(axis, minB, maxB);
			float o1 = maxA - minB;
			float o2 = maxB - minA;
			if (o1 < 0.0f || o2 < 0.0f) { return false; }
			float o = std::min(o1, o2);
			if (o < best.overlap)
			{
				best.overlap = o;
				best.normal = o1 < o2 ? axis : -axis;
			}
			return true;
		}

		// 凸な形同士: 両方の面の向きを分離軸にして重なりを調べ、一番浅い軸で押し返す
		//  面ではなく体積として扱うので、深くめり込んでも押し返す向きが逆にならない
		//  接触点は相手の中に入っている頂点。ないとき（辺と辺だけが当たっている）は小さい方の形の一番出ている点
		//  （大きい床の角などを使うと、接触点が遠く離れて回転ばかりしてしまう）
		bool ConvexConvex(const ConvexProxy& a, const ConvexProxy& b, std::vector<ContactPoint>& out)
		{
			if (a.points.empty() || b.points.empty()) { return false; }
			SatResult best;
			for (const auto& pl : a.planes) { if (!TestConvexAxis(a, b, pl.normal, best)) { return false; } }
			for (const auto& pl : b.planes) { if (!TestConvexAxis(a, b, pl.normal, best)) { return false; } }
			const Vector3& n = best.normal;

			size_t before = out.size();
			float minA, maxA, minB, maxB;
			a.Project(n, minA, maxA);
			b.Project(n, minB, maxB);
			constexpr float kTolerance = 0.01f;
			for (const Vector3& p : a.points)
			{
				float depth = Dot(p, n) - minB;
				if (depth > 0.0f && b.ContainsSideways(p, n, kTolerance)) { out.push_back({ p, n, std::min(depth, best.overlap) }); }
			}
			for (const Vector3& q : b.points)
			{
				float depth = maxA - Dot(q, n);
				if (depth > 0.0f && a.ContainsSideways(q, n, kTolerance)) { out.push_back({ q, n, std::min(depth, best.overlap) }); }
			}
			if (out.size() == before)
			{
				if (a.Radius() <= b.Radius())
				{
					const Vector3* pa = &a.points[0];
					for (const Vector3& p : a.points) { if (Dot(p, n) > Dot(*pa, n)) { pa = &p; } }
					out.push_back({ *pa, n, best.overlap });
				}
				else
				{
					const Vector3* pb = &b.points[0];
					for (const Vector3& q : b.points) { if (Dot(q, n) < Dot(*pb, n)) { pb = &q; } }
					out.push_back({ *pb, n, best.overlap });
				}
			}
			LimitContacts(out, before, 4);
			return true;
		}
	}

	bool BoxConvexMesh(const WorldShape& box, const WorldShape& mesh, std::vector<ContactPoint>& out)
	{
		return mesh.mesh && ConvexConvex(ConvexProxy::FromBox(box), ConvexProxy::FromMesh(mesh), out);
	}

	bool ConvexMeshMesh(const WorldShape& a, const WorldShape& b, std::vector<ContactPoint>& out)
	{
		return ConvexConvex(ConvexProxy::FromMesh(a), ConvexProxy::FromMesh(b), out);
	}

	// 外にいるときは面との最近点（正確）、中に入ったら一番浅い面の向きで押し返す
	bool SphereConvex(const WorldShape& sphere, const WorldShape& mesh, std::vector<ContactPoint>& out)
	{
		ConvexProxy hull = ConvexProxy::FromMesh(mesh);
		if (hull.planes.empty() || !hull.Contains(sphere.center, 0.0f)) { return CollideWithMeshShape(sphere, mesh, out); }
		float best = math::kInfinity;
		Vector3 n;
		for (const auto& pl : hull.planes)
		{
			float pen = pl.d - Dot(sphere.center, pl.normal) + sphere.radius;
			if (pen < best) { best = pen; n = pl.normal; }
		}
		out.push_back({ sphere.center, -n, best });
		return true;
	}

	// 線分が中に入っていなければ面との最近点、入っていれば一番浅い面の向きで両端を押し返す
	bool CapsuleConvex(const WorldShape& capsule, const WorldShape& mesh, std::vector<ContactPoint>& out)
	{
		ConvexProxy hull = ConvexProxy::FromMesh(mesh);
		Vector3 mid = (capsule.p0 + capsule.p1) * 0.5f;
		if (hull.planes.empty() || (!hull.Contains(capsule.p0, 0.0f) && !hull.Contains(capsule.p1, 0.0f) && !hull.Contains(mid, 0.0f)))
		{
			return CollideWithMeshShape(capsule, mesh, out);
		}
		float best = math::kInfinity;
		Vector3 n;
		float d = 0.0f;
		for (const auto& pl : hull.planes)
		{
			float pen = pl.d - std::min(Dot(capsule.p0, pl.normal), Dot(capsule.p1, pl.normal)) + capsule.radius;
			if (pen < best) { best = pen; n = pl.normal; d = pl.d; }
		}
		size_t before = out.size();
		for (const Vector3& e : { capsule.p0, capsule.p1 })
		{
			float pen = d - Dot(e, n) + capsule.radius;
			if (pen > 0.0f) { out.push_back({ e - n * capsule.radius, -n, std::min(pen, best) }); }
		}
		if (out.size() == before) { out.push_back({ mid, -n, best }); }
		return true;
	}
}
