#include "RdPch.h"
#include "Physics/Character/CharacterController.h"
#include "Core/Time/Time.h"
#include "Physics/Collision/ShapeCollision.h"
#include "Scene/Actor/Actor.h"
#include "Scene/Scene.h"

namespace rd
{
	void CharacterController::Reflect(PropertyVisitor& v)
	{
		Collider::Reflect(v);
		v.Property("Radius", radius, { .speed = 0.01f, .min = 0.0f, .max = 1000.0f });
		v.Property("Height", height, { .speed = 0.01f, .min = 0.0f, .max = 1000.0f });
		v.Property("Slope Limit", slopeLimit, { .speed = 0.5f, .min = 0.0f, .max = 90.0f });
		v.Property("Step Offset", stepOffset, { .speed = 0.01f, .min = 0.0f, .max = 100.0f });
		v.Property("Skin Width", skinWidth, { .speed = 0.01f, .min = 0.0f, .max = 10.0f });
	}

	bool CharacterController::UpdateShape(WorldShape& out)
	{
		out = MakeShape(GetTransform().GetPosition());
		return true;
	}

	Vector3 CharacterController::GetApproxSize() const
	{
		return { radius * 2.0f, std::max(height, radius * 2.0f), radius * 2.0f };
	}

	WorldShape CharacterController::MakeShape(const Vector3& position) const
	{
		const Matrix4& m = GetTransform().GetWorldMatrix();
		Vector3 scale = GetAbsScale();
		WorldShape s;
		s.type = ShapeType::Capsule;
		s.center = position + TransformVector(center, m);
		s.radius = radius * std::max(scale.x, scale.z);
		float half = std::max(0.0f, height * scale.y * 0.5f - s.radius);
		s.p0 = s.center - Vector3::kUnitY * half;
		s.p1 = s.center + Vector3::kUnitY * half;
		s.ComputeBounds();
		return s;
	}

	// 自分と子に付いたコライダーには当たらない
	bool CharacterController::IsOwnCollider(const Collider* c) const
	{
		for (Actor* a = c->GetActor(); a; a = a->GetParent())
		{
			if (a == GetActor()) { return true; }
		}
		return false;
	}

	uint32_t CharacterController::Move(const Vector3& motion)
	{
		Vector3 start = GetTransform().GetPosition();
		WorldShape from = MakeShape(start);
		AABB area = from.bounds;
		area.Expand(MakeShape(start + motion).bounds);
		Vector3 margin(from.radius, from.radius, from.radius);
		area = { area.min - margin, area.max + margin };
		GetScene()->GetSystem<PhysicsWorld>().QueryShapes(area, ~0u, mShapes);
		std::erase_if(mShapes, [this](const PhysicsWorld::ShapeQuery& q) { return IsOwnCollider(q.collider); });

		mHits.clear();
		uint32_t flags = kCollisionNone;
		float stepLength = std::max(from.radius * 0.5f, 0.01f);
		int steps = std::clamp(int(std::ceil(Length(motion) / stepLength)), 1, 32);
		Vector3 position = start;
		for (int i = 0; i < steps; ++i)
		{
			position += motion / float(steps);
			flags |= PushOut(position);
		}

		GetTransform().SetPosition(position);
		float dt = Time::DeltaTime();
		mVelocity = dt > 0.0f ? (position - start) / dt : Vector3::kZero;
		mGrounded = (flags & kCollisionBelow) != 0;
		return flags;
	}

	// 1 つのコライダーから複数の接触点が返るので、一番深いものだけで押し戻す
	uint32_t CharacterController::PushOut(Vector3& position)
	{
		uint32_t flags = kCollisionNone;
		float groundDot = std::max(std::cos(math::ToRadians(slopeLimit)), 0.05f);
		std::vector<ContactPoint> contacts;
		for (int iteration = 0; iteration < 4; ++iteration)
		{
			bool moved = false;
			for (const auto& q : mShapes)
			{
				WorldShape me = MakeShape(position);
				if (!me.bounds.Overlaps(q.shape.bounds)) { continue; }
				contacts.clear();
				if (!CollideShapes(me, q.shape, contacts)) { continue; }
				const ContactPoint* deepest = &contacts.front();
				for (const auto& c : contacts) { if (c.depth > deepest->depth) { deepest = &c; } }
				if (deepest->depth <= 1.0e-5f) { continue; }

				Vector3 normal = -deepest->normal;
				if (normal.y >= groundDot)
				{
					position.y += std::min(deepest->depth / normal.y, deepest->depth * 3.0f);
					flags |= kCollisionBelow;
				}
				else
				{
					position += normal * deepest->depth;
					flags |= normal.y < -0.5f ? kCollisionAbove : kCollisionSides;
				}
				moved = true;
				if (std::none_of(mHits.begin(), mHits.end(), [&](const ControllerHit& h) { return h.collider == q.collider; }))
				{
					mHits.push_back({ q.collider, deepest->point, normal });
				}
			}
			if (!moved) { break; }
		}
		return flags;
	}
}
