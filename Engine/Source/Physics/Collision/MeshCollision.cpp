#include "RdPch.h"
#include "Physics/Collision/ClosestPoint.h"
#include "Physics/Collision/CollisionDetail.h"
#include "Physics/Collision/ConvexProxy.h"

namespace rd::collision
{
	namespace
	{
		// 形をメッシュのローカル座標へ移す（回転と移動だけなので形は変わらない）
		WorldShape ToMeshSpace(const WorldShape& s, const WorldShape& mesh)
		{
			WorldShape local = s;
			Quaternion inv = Conjugate(mesh.meshRotation);
			local.center = mesh.WorldToMesh(s.center);
			local.p0 = mesh.WorldToMesh(s.p0);
			local.p1 = mesh.WorldToMesh(s.p1);
			for (auto& axis : local.axes) { axis = axis * inv; }
			local.ComputeBounds();
			return local;
		}

		void SphereTriangle(const WorldShape& s, const Vector3& a, const Vector3& b, const Vector3& c, std::vector<ContactPoint>& out)
		{
			Vector3 q = ClosestPointOnTriangle(s.center, a, b, c);
			Vector3 d = q - s.center;
			float dist2 = LengthSq(d);
			if (dist2 < s.radius * s.radius && dist2 > 1.0e-12f)
			{
				float dist = std::sqrt(dist2);
				out.push_back({ q, d / dist, s.radius - dist });
			}
		}

		// 線分が面に触れているときは、面の法線の向き（カプセル → 面）で押し返す
		void CapsuleTriangle(const WorldShape& s, const Vector3& a, const Vector3& b, const Vector3& c, std::vector<ContactPoint>& out)
		{
			Vector3 onSeg, onTri;
			ClosestSegmentTriangle(s.p0, s.p1, a, b, c, onSeg, onTri);
			Vector3 d = onTri - onSeg;
			float dist2 = LengthSq(d);
			if (dist2 < s.radius * s.radius)
			{
				float dist = std::sqrt(dist2);
				Vector3 n = dist > 1.0e-6f ? d / dist : Normalize(Cross(b - a, c - a));
				out.push_back({ onTri, n, s.radius - dist });
			}
		}

		bool CollideWithMesh(const WorldShape& s, const MeshCollisionData& mesh, std::vector<ContactPoint>& out)
		{
			size_t before = out.size();
			mesh.Query(s.bounds, [&](uint32_t tri)
				{
					Vector3 a, b, c;
					mesh.GetTriangle(tri, a, b, c);
					switch (s.type)
					{
					case ShapeType::Sphere: SphereTriangle(s, a, b, c, out); break;
					case ShapeType::Capsule: CapsuleTriangle(s, a, b, c, out); break;
					case ShapeType::Box: BoxTriangle(s, a, b, c, out); break;
					default: break;
					}
				});
			LimitContacts(out, before, 4);
			return out.size() > before;
		}

		// from の頂点が into の面より内側に入っていたら接触とみなす
		//  margin より深いと裏側の面と区別がつかないので、それより浅いものだけを拾う。頂点が多いメッシュは間引く
		//  into の面は from を外向き (+n) に押し返すので、from が A なら法線は -n
		void MeshVerticesIntoMesh(const WorldShape& from, const WorldShape& into, bool flip, std::vector<ContactPoint>& out)
		{
			const auto& verts = from.mesh->GetPositions();
			if (verts.empty()) { return; }
			Vector3 size = from.mesh->GetBounds().GetExtents();
			float margin = std::clamp(std::min(size.x, std::min(size.y, size.z)) * 0.5f, 0.02f, 0.5f);
			AABB intoBounds = into.bounds;
			intoBounds.min -= Vector3(margin, margin, margin);
			intoBounds.max += Vector3(margin, margin, margin);
			size_t step = std::max<size_t>(1, verts.size() / 512);
			for (size_t i = 0; i < verts.size(); i += step)
			{
				Vector3 world = from.MeshToWorld(verts[i]);
				if (!intoBounds.Contains(world)) { continue; }
				Vector3 p = into.WorldToMesh(world);
				float bestDist = margin;
				Vector3 bestPoint, bestNormal;
				bool found = false;
				into.mesh->Query(AABB::FromCenterExtents(p, Vector3(margin, margin, margin)), [&](uint32_t tri)
					{
						Vector3 a, b, c;
						into.mesh->GetTriangle(tri, a, b, c);
						Vector3 n = Cross(c - a, b - a);
						float len = Length(n);
						if (len < 1.0e-12f) { return; }
						n /= len;
						Vector3 q = ClosestPointOnTriangle(p, a, b, c);
						float behind = Dot(q - p, n);
						float dist = Length(q - p);
						if (behind > 0.0f && dist < bestDist)
						{
							bestDist = dist;
							bestPoint = q;
							bestNormal = n;
							found = true;
						}
					});
				if (!found) { continue; }
				Vector3 n = bestNormal * into.meshRotation;
				out.push_back({ into.MeshToWorld(bestPoint), flip ? n : -n, bestDist });
			}
		}
	}

	// 動いているメッシュでも、相手をメッシュのローカル座標に移して調べ、結果をワールドへ戻す
	bool CollideWithMeshShape(const WorldShape& s, const WorldShape& meshShape, std::vector<ContactPoint>& out)
	{
		if (!meshShape.mesh) { return false; }
		size_t before = out.size();
		if (!CollideWithMesh(ToMeshSpace(s, meshShape), *meshShape.mesh, out)) { return false; }
		for (size_t i = before; i < out.size(); ++i)
		{
			out[i].point = meshShape.MeshToWorld(out[i].point);
			out[i].normal = out[i].normal * meshShape.meshRotation;
		}
		return true;
	}

	// メッシュ同士: 両方が凸包なら凸包同士、そうでなければ頂点が相手の面より内側に入ったかを両方向に調べる
	//  （辺と辺だけが交差する場合は拾えないが、箱のような物を地形に置く用途には十分）
	bool MeshMesh(const WorldShape& a, const WorldShape& b, std::vector<ContactPoint>& out)
	{
		if (a.mesh && b.mesh && a.meshConvex && b.meshConvex) { return ConvexMeshMesh(a, b, out); }
		if (!a.mesh || !b.mesh) { return false; }
		size_t before = out.size();
		MeshVerticesIntoMesh(a, b, false, out);
		MeshVerticesIntoMesh(b, a, true, out);
		LimitContacts(out, before, 4);
		return out.size() > before;
	}
}
