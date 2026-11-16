#include "RdPch.h"
#include "Physics/Colliders/SphereCollider.h"

namespace rd
{
	void SphereCollider::Reflect(PropertyVisitor& v)
	{
		Collider::Reflect(v);
		v.Property("Radius", radius, { .speed = 0.01f, .min = 0.0f, .max = 1000.0f });
	}

	bool SphereCollider::UpdateShape(WorldShape& out)
	{
		Vector3 s = GetAbsScale();
		out.type = ShapeType::Sphere;
		out.center = GetWorldCenter();
		out.radius = radius * std::max(s.x, std::max(s.y, s.z));
		out.ComputeBounds();
		return true;
	}
}
