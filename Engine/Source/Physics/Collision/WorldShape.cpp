#include "RdPch.h"
#include "Physics/Collision/WorldShape.h"

namespace rd
{
	void WorldShape::ComputeBounds()
	{
		switch (type)
		{
		case ShapeType::Sphere:
			bounds = AABB::FromCenterExtents(center, Vector3(radius, radius, radius));
			break;
		case ShapeType::Box:
		{
			Vector3 e;
			for (int i = 0; i < 3; ++i) { e += Abs(axes[i]) * halfExtents[i]; }
			bounds = AABB::FromCenterExtents(center, e);
			break;
		}
		case ShapeType::Capsule:
			bounds = AABB(Min(p0, p1) - Vector3(radius, radius, radius), Max(p0, p1) + Vector3(radius, radius, radius));
			break;
		case ShapeType::Mesh:
			bounds = mesh ? mesh->GetBounds().Transform(Matrix4::CreateRotate(meshRotation) * Matrix4::CreateTranslate(meshPosition)) : AABB();
			break;
		}
	}
}
