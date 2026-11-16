#include "RdPch.h"
#include "Physics/Colliders/Collider.h"
#include "Physics/Rigidbody/Rigidbody.h"
#include "Physics/World/PhysicsWorld.h"
#include "Render/Debug/DebugDraw.h"
#include "Scene/Actor/Actor.h"
#include "Scene/Scene.h"

namespace rd
{
	void Collider::Reflect(PropertyVisitor& v)
	{
		v.Property("Center", center);
		v.Property("Is Trigger", isTrigger);
		v.Property("Friction", friction, { .speed = 0.01f, .min = 0.0f, .max = 2.0f });
		v.Property("Restitution", restitution, { .speed = 0.01f, .min = 0.0f, .max = 1.0f });
	}

	void Collider::Awake()
	{
		GetScene()->GetSystem<PhysicsWorld>().Register(this);
	}

	void Collider::OnDestroy()
	{
		GetScene()->GetSystem<PhysicsWorld>().Unregister(this);
	}

	Rigidbody* Collider::GetAttachedRigidbody() const
	{
		for (Actor* a = GetActor(); a; a = a->GetParent())
		{
			if (auto rb = a->GetComponent<Rigidbody>(); rb && rb->IsEnabled()) { return rb; }
		}
		return nullptr;
	}

	Vector3 Collider::GetWorldCenter() const
	{
		return TransformPoint(center, GetTransform().GetWorldMatrix());
	}

	Vector3 Collider::GetAbsScale() const
	{
		return Abs(GetTransform().GetLossyScale());
	}

#if RD_EDITOR
	void Collider::OnDrawGizmos(DebugDraw& draw, bool selected)
	{
		WorldShape s;
		if (!UpdateShape(s)) { return; }
		float alpha = selected ? 1.0f : 0.5f;
		Color color = isTrigger ? Color(0.3f, 0.8f, 1.0f, alpha) : Color(0.3f, 1.0f, 0.4f, alpha);
		switch (s.type)
		{
		case ShapeType::Sphere:
			draw.Sphere(s.center, s.radius, color);
			break;
		case ShapeType::Box:
		{
			Vector3 size = s.halfExtents * 2.0f;
			Matrix4 m(
				s.axes[0].x * size.x, s.axes[0].y * size.x, s.axes[0].z * size.x, 0,
				s.axes[1].x * size.y, s.axes[1].y * size.y, s.axes[1].z * size.y, 0,
				s.axes[2].x * size.z, s.axes[2].y * size.z, s.axes[2].z * size.z, 0,
				s.center.x, s.center.y, s.center.z, 1);
			draw.Box(m, color);
			break;
		}
		case ShapeType::Capsule:
			draw.Capsule(s.p0, s.p1, s.radius, color);
			break;
		case ShapeType::Mesh:
			if (selected) { draw.Box(s.bounds, color); }
			break;
		}
	}
#endif
}
