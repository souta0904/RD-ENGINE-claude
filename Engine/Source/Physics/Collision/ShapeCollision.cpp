#include "RdPch.h"
#include "Physics/Collision/ShapeCollision.h"
#include "Physics/Collision/ClosestPoint.h"
#include "Physics/Collision/CollisionDetail.h"
#include "Math/Geometry/Intersection.h"

namespace rd
{
	using namespace collision;

	namespace
	{
		// 箱とカプセルは、カプセルの軸の最近点を球として扱う（SphereBox の法線は球 → 箱なので反転する）
		bool BoxCapsule(const WorldShape& box, const WorldShape& capsule, std::vector<ContactPoint>& out)
		{
			Vector3 onSeg, onBox;
			ClosestSegmentBox(capsule.p0, capsule.p1, box, onSeg, onBox);
			size_t before = out.size();
			if (!SphereBox(onSeg, capsule.radius, box, out)) { return false; }
			for (size_t i = before; i < out.size(); ++i) { out[i].normal = -out[i].normal; }
			return true;
		}

		bool SphereShape(const WorldShape& a, const WorldShape& b, std::vector<ContactPoint>& out)
		{
			switch (b.type)
			{
			case ShapeType::Sphere: return SphereSphere(a.center, a.radius, b.center, b.radius, out);
			case ShapeType::Box: return SphereBox(a.center, a.radius, b, out);
			case ShapeType::Capsule: return SphereSphere(a.center, a.radius, ClosestPointOnSegment(a.center, b.p0, b.p1), b.radius, out);
			case ShapeType::Mesh: return b.meshConvex ? SphereConvex(a, b, out) : CollideWithMeshShape(a, b, out);
			}
			return false;
		}

		bool BoxShape(const WorldShape& a, const WorldShape& b, std::vector<ContactPoint>& out)
		{
			switch (b.type)
			{
			case ShapeType::Box: return BoxBox(a, b, out);
			case ShapeType::Capsule: return BoxCapsule(a, b, out);
			case ShapeType::Mesh: return b.meshConvex ? BoxConvexMesh(a, b, out) : CollideWithMeshShape(a, b, out);
			default: return false;
			}
		}

		bool CapsuleShape(const WorldShape& a, const WorldShape& b, std::vector<ContactPoint>& out)
		{
			switch (b.type)
			{
			case ShapeType::Capsule:
			{
				Vector3 c1, c2;
				ClosestPointsSegmentSegment(a.p0, a.p1, b.p0, b.p1, c1, c2);
				return SphereSphere(c1, a.radius, c2, b.radius, out);
			}
			case ShapeType::Mesh: return b.meshConvex ? CapsuleConvex(a, b, out) : CollideWithMeshShape(a, b, out);
			default: return false;
			}
		}

		// a.type <= b.type のときだけ呼ぶ（組み合わせの数を半分にするため）
		bool CollideOrdered(const WorldShape& a, const WorldShape& b, std::vector<ContactPoint>& out)
		{
			switch (a.type)
			{
			case ShapeType::Sphere: return SphereShape(a, b, out);
			case ShapeType::Box: return BoxShape(a, b, out);
			case ShapeType::Capsule: return CapsuleShape(a, b, out);
			case ShapeType::Mesh: return b.type == ShapeType::Mesh && MeshMesh(a, b, out);
			}
			return false;
		}

		bool RaySphere(const Ray& ray, const Vector3& c, float r, float& t)
		{
			Vector3 m = ray.origin - c;
			float b = Dot(m, ray.direction);
			float cc = Dot(m, m) - r * r;
			if (cc > 0.0f && b > 0.0f) { return false; }
			float disc = b * b - cc;
			if (disc < 0.0f) { return false; }
			t = std::max(0.0f, -b - std::sqrt(disc));
			return true;
		}

		// 箱のローカル空間でスラブ判定し、当たった点が一番外に出ている軸を法線にする
		bool RaycastBox(const WorldShape& s, const Ray& ray, float maxDist, float& outT, Vector3& outNormal)
		{
			float t = 0.0f;
			Vector3 o = ray.origin - s.center;
			Ray local{ { Dot(o, s.axes[0]), Dot(o, s.axes[1]), Dot(o, s.axes[2]) },
				{ Dot(ray.direction, s.axes[0]), Dot(ray.direction, s.axes[1]), Dot(ray.direction, s.axes[2]) } };
			AABB box(-s.halfExtents, s.halfExtents);
			if (!RayIntersectsAABB(local, box, t) || t > maxDist) { return false; }
			Vector3 p = local.GetPoint(t);
			int axis = 0;
			float best = 0.0f;
			for (int i = 0; i < 3; ++i)
			{
				float d = std::fabs(p[i]) / std::max(s.halfExtents[i], 1.0e-6f);
				if (d > best) { best = d; axis = i; }
			}
			outT = t;
			outNormal = s.axes[axis] * (p[axis] >= 0.0f ? 1.0f : -1.0f);
			return true;
		}

		// 両端の球と、軸に垂直な成分だけで解いた円柱の部分のうち一番近いもの
		bool RaycastCapsule(const WorldShape& s, const Ray& ray, float maxDist, float& outT, Vector3& outNormal)
		{
			float t = 0.0f;
			bool hit = false;
			float best = maxDist;
			Vector3 normal;
			for (const Vector3& c : { s.p0, s.p1 })
			{
				if (RaySphere(ray, c, s.radius, t) && t < best) { best = t; normal = Normalize(ray.GetPoint(t) - c); hit = true; }
			}
			Vector3 axis = s.p1 - s.p0;
			float len = Length(axis);
			if (len > 1.0e-6f)
			{
				Vector3 n = axis / len;
				Vector3 m = ray.origin - s.p0;
				Vector3 dp = ray.direction - n * Dot(ray.direction, n);
				Vector3 mp = m - n * Dot(m, n);
				float a = Dot(dp, dp), b = 2.0f * Dot(dp, mp), c = Dot(mp, mp) - s.radius * s.radius;
				float disc = b * b - 4.0f * a * c;
				if (a > 1.0e-8f && disc >= 0.0f)
				{
					float tc = (-b - std::sqrt(disc)) / (2.0f * a);
					if (tc >= 0.0f && tc < best)
					{
						float h = Dot(ray.GetPoint(tc) - s.p0, n);
						if (h >= 0.0f && h <= len)
						{
							best = tc;
							normal = Normalize(ray.GetPoint(tc) - (s.p0 + n * h));
							hit = true;
						}
					}
				}
			}
			if (hit) { outT = best; outNormal = normal; }
			return hit;
		}
	}

	bool CollideShapes(const WorldShape& a, const WorldShape& b, std::vector<ContactPoint>& out)
	{
		if (!a.bounds.Overlaps(b.bounds)) { return false; }
		if (a.type <= b.type) { return CollideOrdered(a, b, out); }
		size_t before = out.size();
		bool hit = CollideOrdered(b, a, out);
		for (size_t i = before; i < out.size(); ++i) { out[i].normal = -out[i].normal; }
		return hit;
	}

	bool RaycastShape(const WorldShape& s, const Ray& ray, float maxDist, float& outT, Vector3& outNormal)
	{
		switch (s.type)
		{
		case ShapeType::Sphere:
		{
			float t = 0.0f;
			if (!RaySphere(ray, s.center, s.radius, t) || t > maxDist) { return false; }
			outT = t;
			outNormal = Normalize(ray.GetPoint(t) - s.center);
			return true;
		}
		case ShapeType::Box:
			return RaycastBox(s, ray, maxDist, outT, outNormal);
		case ShapeType::Capsule:
			return RaycastCapsule(s, ray, maxDist, outT, outNormal);
		case ShapeType::Mesh:
		{
			if (!s.mesh) { return false; }
			// 回転と移動だけなので、メッシュのローカル座標でも距離は変わらない
			Ray local{ s.WorldToMesh(ray.origin), ray.direction * Conjugate(s.meshRotation) };
			if (!s.mesh->Raycast(local, maxDist, outT, outNormal)) { return false; }
			outNormal = outNormal * s.meshRotation;
			return true;
		}
		}
		return false;
	}
}
