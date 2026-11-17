#include "RdPch.h"
#include "Physics/World/PhysicsWorld.h"
#include "Core/Profiling/ProfileScope.h"
#include "Math/Geometry/Intersection.h"
#include "Physics/Collision/ShapeCollision.h"
#include "Physics/Colliders/Collider.h"
#include "Physics/Rigidbody/Rigidbody.h"
#include "Scene/Actor/Actor.h"
#include "Scene/Scene.h"

namespace rd
{
	namespace
	{
		bool InLayerMask(uint32_t mask, const Collider* c)
		{
			return (mask >> c->GetActor()->GetLayer()) & 1u;
		}
	}

	PhysicsWorld::PhysicsWorld()
	{
		for (auto& m : mLayerMask) { m = ~0u; }
	}

	void PhysicsWorld::Register(Collider* collider)
	{
		if (std::find(mColliders.begin(), mColliders.end(), collider) == mColliders.end())
		{
			mColliders.push_back(collider);
		}
	}

	// 消えたコライダーを含むペアも捨てる（Exit は送らない）
	void PhysicsWorld::Unregister(Collider* collider)
	{
		mColliders.erase(std::remove(mColliders.begin(), mColliders.end(), collider), mColliders.end());
		for (auto it = mPrevPairs.begin(); it != mPrevPairs.end();)
		{
			if (it->first.first == collider || it->first.second == collider) { it = mPrevPairs.erase(it); }
			else { ++it; }
		}
	}

	void PhysicsWorld::SetLayerCollision(uint32_t a, uint32_t b, bool collide)
	{
		a &= 31; b &= 31;
		if (collide) { mLayerMask[a] |= 1u << b; mLayerMask[b] |= 1u << a; }
		else { mLayerMask[a] &= ~(1u << b); mLayerMask[b] &= ~(1u << a); }
	}

	void PhysicsWorld::Step(Scene& scene, float dt)
	{
		RD_PROFILE_SCOPE("Physics");
		mStates.clear();
		IntegrateForces(scene, dt);

		BuildEntries();
		std::vector<Manifold> manifolds;
		FindContacts(manifolds);
		mLastContactCount = uint32_t(manifolds.size());

		SolveVelocities(manifolds);
		IntegratePositions(dt);
		ResolvePenetration(manifolds);
		WriteBackTransforms();
		DispatchEvents(manifolds);
	}

	// 力と重力で速度を変え、抵抗で減らす。慣性テンソルは最初のコライダーの大きさを箱とみなして近似する
	void PhysicsWorld::IntegrateForces(Scene& scene, float dt)
	{
		const Vector3 gravity = scene.GetGravity();
		for (Rigidbody* rb : scene.FindComponents<Rigidbody>())
		{
			Transform& t = rb->GetTransform();
			mStates[rb] = { t.GetPosition(), t.GetRotation() };
			rb->mGrounded = false;

			Vector3 size = Vector3::kOne;
			if (auto col = rb->GetComponent<Collider>()) { size = col->GetApproxSize() * Abs(t.GetLossyScale()); }
			if (rb->freezeRotation || !rb->IsDynamic())
			{
				rb->mInvInertiaLocal = Vector3::kZero;
			}
			else
			{
				Vector3 s2 = size * size;
				Vector3 inertia = Vector3(s2.y + s2.z, s2.x + s2.z, s2.x + s2.y) * (rb->mass / 12.0f);
				rb->mInvInertiaLocal = Vector3(1.0f / std::max(inertia.x, 1e-6f), 1.0f / std::max(inertia.y, 1e-6f), 1.0f / std::max(inertia.z, 1e-6f));
			}

			if (rb->IsDynamic())
			{
				Vector3 accel = rb->mForce / rb->mass;
				if (rb->useGravity) { accel += gravity; }
				rb->velocity += accel * dt;
				rb->velocity *= 1.0f / (1.0f + rb->drag * dt);
				if (!rb->freezeRotation)
				{
					Vector3 local = rb->mTorque * Conjugate(t.GetRotation());
					rb->angularVelocity += (local * rb->mInvInertiaLocal) * t.GetRotation() * dt;
					rb->angularVelocity *= 1.0f / (1.0f + rb->angularDrag * dt);
				}
				else
				{
					rb->angularVelocity = Vector3::kZero;
				}
			}
			rb->mForce = Vector3::kZero;
			rb->mTorque = Vector3::kZero;
		}
	}

	void PhysicsWorld::BuildEntries()
	{
		mEntries.clear();
		mEntries.reserve(mColliders.size());
		for (Collider* c : mColliders)
		{
			if (!c->IsActiveAndEnabled()) { continue; }
			Entry e;
			e.collider = c;
			e.body = c->GetAttachedRigidbody();
			if (e.body && !e.body->IsActiveAndEnabled()) { e.body = nullptr; }
			e.layer = c->GetActor()->GetLayer();
			e.valid = c->UpdateShape(e.shape);
			if (e.valid) { mEntries.push_back(e); }
		}
	}

	// X 軸の始まりで並べ、X の範囲が重なるものだけを細かく調べる（スイープ & プルーン）
	//  静的同士・同じ剛体・キネマティック同士（トリガーでなければ）は調べない
	void PhysicsWorld::FindContacts(std::vector<Manifold>& manifolds)
	{
		std::vector<Entry*> sorted;
		sorted.reserve(mEntries.size());
		for (auto& e : mEntries) { sorted.push_back(&e); }
		std::sort(sorted.begin(), sorted.end(), [](Entry* a, Entry* b) { return a->shape.bounds.min.x < b->shape.bounds.min.x; });

		std::vector<ContactPoint> contacts;
		for (size_t i = 0; i < sorted.size(); ++i)
		{
			Entry* a = sorted[i];
			for (size_t j = i + 1; j < sorted.size(); ++j)
			{
				Entry* b = sorted[j];
				if (b->shape.bounds.min.x > a->shape.bounds.max.x) { break; }
				if (!a->body && !b->body) { continue; }
				if (a->body && a->body == b->body) { continue; }
				if (!GetLayerCollision(a->layer, b->layer)) { continue; }
				bool trigger = a->collider->isTrigger || b->collider->isTrigger;
				bool aDynamic = a->body && a->body->IsDynamic();
				bool bDynamic = b->body && b->body->IsDynamic();
				if (!trigger && !aDynamic && !bDynamic) { continue; }

				contacts.clear();
				if (!CollideShapes(a->shape, b->shape, contacts)) { continue; }
				Manifold m;
				m.a = a;
				m.b = b;
				m.trigger = trigger;
				m.contacts = contacts;
				m.normalImpulse.assign(contacts.size(), 0.0f);
				m.tangentImpulse.assign(contacts.size() * 2, 0.0f);
				manifolds.push_back(std::move(m));
			}
		}
	}

	bool PhysicsWorld::Raycast(const Ray& ray, float maxDistance, RaycastHit& hit, uint32_t layerMask, bool hitTriggers)
	{
		bool found = false;
		float best = maxDistance;
		for (Collider* c : mColliders)
		{
			if (!c->IsActiveAndEnabled()) { continue; }
			if (c->isTrigger && !hitTriggers) { continue; }
			if (!InLayerMask(layerMask, c)) { continue; }
			WorldShape shape;
			if (!c->UpdateShape(shape)) { continue; }
			float tBox;
			if (!RayIntersectsAABB(ray, shape.bounds, tBox) || tBox > best) { continue; }
			float t;
			Vector3 n;
			if (RaycastShape(shape, ray, best, t, n) && t <= best)
			{
				best = t;
				hit = { c, ray.GetPoint(t), n, t };
				found = true;
			}
		}
		return found;
	}

	void PhysicsWorld::QueryShapes(const AABB& area, uint32_t layerMask, std::vector<ShapeQuery>& out)
	{
		out.clear();
		for (Collider* c : mColliders)
		{
			if (!c->IsActiveAndEnabled() || c->isTrigger || !InLayerMask(layerMask, c)) { continue; }
			ShapeQuery q;
			q.collider = c;
			if (c->UpdateShape(q.shape) && q.shape.bounds.Overlaps(area)) { out.push_back(q); }
		}
	}

	std::vector<Collider*> PhysicsWorld::OverlapSphere(const Vector3& center, float radius, uint32_t layerMask)
	{
		std::vector<Collider*> result;
		WorldShape probe;
		probe.type = ShapeType::Sphere;
		probe.center = center;
		probe.radius = radius;
		probe.ComputeBounds();
		std::vector<ContactPoint> tmp;
		for (Collider* c : mColliders)
		{
			if (!c->IsActiveAndEnabled() || !InLayerMask(layerMask, c)) { continue; }
			WorldShape shape;
			if (!c->UpdateShape(shape)) { continue; }
			tmp.clear();
			if (CollideShapes(probe, shape, tmp)) { result.push_back(c); }
		}
		return result;
	}
}
