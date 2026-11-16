#include "RdPch.h"
#include "Physics/Collision/ConvexProxy.h"
#include "Physics/Collision/CollisionDetail.h"

namespace rd
{
	ConvexProxy ConvexProxy::FromBox(const WorldShape& box)
	{
		ConvexProxy c;
		c.points.resize(8);
		collision::BoxVertices(box, c.points.data());
		for (int i = 0; i < 3; ++i)
		{
			float center = Dot(box.center, box.axes[i]);
			c.planes.push_back({ box.axes[i], center + box.halfExtents[i] });
			c.planes.push_back({ -box.axes[i], -center + box.halfExtents[i] });
		}
		return c;
	}

	ConvexProxy ConvexProxy::FromMesh(const WorldShape& mesh)
	{
		ConvexProxy c;
		for (const Vector3& p : mesh.mesh->GetHullPoints()) { c.points.push_back(mesh.MeshToWorld(p)); }
		for (const auto& pl : mesh.mesh->GetHullPlanes())
		{
			Vector3 n = pl.normal * mesh.meshRotation;
			c.planes.push_back({ n, pl.d + Dot(n, mesh.meshPosition) });
		}
		return c;
	}

	bool ConvexProxy::Contains(const Vector3& p, float tolerance) const
	{
		for (const auto& pl : planes)
		{
			if (Dot(p, pl.normal) > pl.d + tolerance) { return false; }
		}
		return true;
	}

	bool ConvexProxy::ContainsSideways(const Vector3& p, const Vector3& n, float tolerance) const
	{
		for (const auto& pl : planes)
		{
			if (std::fabs(Dot(pl.normal, n)) > 0.7f) { continue; }
			if (Dot(p, pl.normal) > pl.d + tolerance) { return false; }
		}
		return true;
	}

	float ConvexProxy::Radius() const
	{
		Vector3 c;
		for (const Vector3& p : points) { c += p; }
		c /= float(std::max<size_t>(points.size(), 1));
		float r = 0.0f;
		for (const Vector3& p : points) { r = std::max(r, LengthSq(p - c)); }
		return std::sqrt(r);
	}

	void ConvexProxy::Project(const Vector3& axis, float& mn, float& mx) const
	{
		mn = math::kInfinity;
		mx = -math::kInfinity;
		for (const Vector3& p : points)
		{
			float d = Dot(p, axis);
			mn = std::min(mn, d);
			mx = std::max(mx, d);
		}
	}
}
