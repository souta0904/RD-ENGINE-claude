#include "RdPch.h"
#include "Physics/PhysicsWorld.h"
#include "Core/Profiler.h"
#include "Physics/Collider.h"
#include "Physics/Rigidbody.h"
#include "Scene/Actor.h"
#include "Scene/Scene.h"

namespace rd
{
	namespace
	{
		constexpr float kPenetrationSlop = 0.005f;
		constexpr float kCorrectionPercent = 0.6f;
		constexpr float kRestitutionThreshold = 1.0f;// これより遅い衝突では跳ね返らない
		constexpr float kGroundCos = 0.7f;// これより上向きの面を地面とみなす

		struct BodyState
		{
			Vector3 position;
			Quaternion rotation;
		};
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

	void PhysicsWorld::Unregister(Collider* collider)
	{
		mColliders.erase(std::remove(mColliders.begin(), mColliders.end(), collider), mColliders.end());
		// 破棄されたコライダーを含むペアを捨てる（Exit は呼ばない）
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

	void PhysicsWorld::Step(Scene& scene, float dt)
	{
		RD_PROFILE_SCOPE("Physics");
		std::unordered_map<Rigidbody*, BodyState> states;
		const Vector3 gravity = scene.GetGravity();

		// ---------------- 1. 力の積分 ----------------
		for (Rigidbody* rb : scene.FindComponents<Rigidbody>())
		{
			Transform& t = rb->GetTransform();
			states[rb] = { t.GetPosition(), t.GetRotation() };
			rb->mGrounded = false;

			// 慣性テンソル（最初のコライダーの大きさから近似）
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

		// ---------------- 2. ブロードフェーズ ----------------
		BuildEntries();
		std::vector<Entry*> sorted;
		sorted.reserve(mEntries.size());
		for (auto& e : mEntries) { sorted.push_back(&e); }
		std::sort(sorted.begin(), sorted.end(), [](Entry* a, Entry* b) { return a->shape.bounds.min.x < b->shape.bounds.min.x; });

		std::vector<Manifold> manifolds;
		std::vector<ContactPoint> contacts;
		for (size_t i = 0; i < sorted.size(); ++i)
		{
			Entry* a = sorted[i];
			for (size_t j = i + 1; j < sorted.size(); ++j)
			{
				Entry* b = sorted[j];
				if (b->shape.bounds.min.x > a->shape.bounds.max.x) { break; }// X 軸で離れたら以降も離れている
				if (!a->body && !b->body) { continue; }// 静的同士
				if (a->body && a->body == b->body) { continue; }// 同じ剛体
				if (!GetLayerCollision(a->layer, b->layer)) { continue; }
				bool trigger = a->collider->isTrigger || b->collider->isTrigger;
				bool aDynamic = a->body && a->body->IsDynamic();
				bool bDynamic = b->body && b->body->IsDynamic();
				if (!trigger && !aDynamic && !bDynamic) { continue; }// キネマティック同士

				// ---------------- 3. 狭域判定 ----------------
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
		mLastContactCount = uint32_t(manifolds.size());

		// ---------------- 4. 速度の解決 ----------------
		auto rotationOf = [&](Rigidbody* rb) { auto it = states.find(rb); return it != states.end() ? it->second.rotation : Quaternion::kIdentity; };
		auto positionOf = [&](Rigidbody* rb) { auto it = states.find(rb); return it != states.end() ? it->second.position : Vector3::kZero; };
		auto invInertia = [&](Rigidbody* rb, const Vector3& v) -> Vector3
			{
				if (!rb || !rb->IsDynamic()) { return Vector3::kZero; }
				Quaternion q = rotationOf(rb);
				return ((v * Conjugate(q)) * rb->mInvInertiaLocal) * q;
			};
		auto velocityAt = [&](Rigidbody* rb, const Vector3& r) -> Vector3
			{
				return rb ? rb->velocity + Cross(rb->angularVelocity, r) : Vector3::kZero;
			};
		auto applyImpulse = [&](Rigidbody* rb, const Vector3& r, const Vector3& impulse)
			{
				if (!rb || !rb->IsDynamic()) { return; }
				rb->velocity += impulse * rb->GetInverseMass();
				rb->angularVelocity += invInertia(rb, Cross(r, impulse));
			};

		// 跳ね返りの目標速度を先に求めておく
		struct ContactCache { Vector3 rA, rB, t1, t2; float bounce, kn, kt1, kt2, mu; };
		std::vector<std::vector<ContactCache>> caches(manifolds.size());
		for (size_t mi = 0; mi < manifolds.size(); ++mi)
		{
			Manifold& m = manifolds[mi];
			if (m.trigger) { continue; }
			Rigidbody* ra = m.a->body;
			Rigidbody* rbB = m.b->body;
			float invMA = ra ? ra->GetInverseMass() : 0.0f;
			float invMB = rbB ? rbB->GetInverseMass() : 0.0f;
			float mu = std::sqrt(m.a->collider->friction * m.b->collider->friction);
			float e = std::max(m.a->collider->restitution, m.b->collider->restitution);
			for (auto& c : m.contacts)
			{
				ContactCache cc;
				cc.rA = ra ? c.point - positionOf(ra) : Vector3::kZero;
				cc.rB = rbB ? c.point - positionOf(rbB) : Vector3::kZero;
				const Vector3& n = c.normal;
				auto effMass = [&](const Vector3& dir)
					{
						float k = invMA + invMB;
						k += Dot(dir, Cross(invInertia(ra, Cross(cc.rA, dir)), cc.rA));
						k += Dot(dir, Cross(invInertia(rbB, Cross(cc.rB, dir)), cc.rB));
						return k > 1.0e-8f ? 1.0f / k : 0.0f;
					};
				cc.kn = effMass(n);
				cc.t1 = Normalize(Cross(std::fabs(n.y) < 0.9f ? Vector3::kUnitY : Vector3::kUnitX, n));
				cc.t2 = Cross(n, cc.t1);
				cc.kt1 = effMass(cc.t1);
				cc.kt2 = effMass(cc.t2);
				cc.mu = mu;
				float vn = Dot(velocityAt(rbB, cc.rB) - velocityAt(ra, cc.rA), n);
				cc.bounce = vn < -kRestitutionThreshold ? -e * vn : 0.0f;
				caches[mi].push_back(cc);
			}
		}

		for (int iter = 0; iter < solverIterations; ++iter)
		{
			for (size_t mi = 0; mi < manifolds.size(); ++mi)
			{
				Manifold& m = manifolds[mi];
				if (m.trigger) { continue; }
				Rigidbody* ra = m.a->body;
				Rigidbody* rbB = m.b->body;
				for (size_t ci = 0; ci < m.contacts.size(); ++ci)
				{
					const ContactCache& cc = caches[mi][ci];
					const Vector3& n = m.contacts[ci].normal;

					// 法線方向
					Vector3 vrel = velocityAt(rbB, cc.rB) - velocityAt(ra, cc.rA);
					float dj = (cc.bounce - Dot(vrel, n)) * cc.kn;
					float old = m.normalImpulse[ci];
					m.normalImpulse[ci] = std::max(old + dj, 0.0f);
					dj = m.normalImpulse[ci] - old;
					applyImpulse(ra, cc.rA, -n * dj);
					applyImpulse(rbB, cc.rB, n * dj);

					// 摩擦（2 方向）
					float maxF = cc.mu * m.normalImpulse[ci];
					const Vector3 tangents[2] = { cc.t1, cc.t2 };
					const float kts[2] = { cc.kt1, cc.kt2 };
					for (int k = 0; k < 2; ++k)
					{
						vrel = velocityAt(rbB, cc.rB) - velocityAt(ra, cc.rA);
						float djt = -Dot(vrel, tangents[k]) * kts[k];
						float& acc = m.tangentImpulse[ci * 2 + size_t(k)];
						float prev = acc;
						acc = math::Clamp(prev + djt, -maxF, maxF);
						djt = acc - prev;
						applyImpulse(ra, cc.rA, -tangents[k] * djt);
						applyImpulse(rbB, cc.rB, tangents[k] * djt);
					}
				}
			}
		}

		// ---------------- 5. 位置の積分 ----------------
		for (auto& [rb, st] : states)
		{
			if (!rb->IsDynamic()) { continue; }
			st.position += rb->velocity * dt;
			if (LengthSq(rb->angularVelocity) > 1.0e-12f)
			{
				Quaternion w(0.0f, rb->angularVelocity.x, rb->angularVelocity.y, rb->angularVelocity.z);
				Quaternion dq = st.rotation * w;
				st.rotation = Normalize(Quaternion(
					st.rotation.w + dq.w * 0.5f * dt, st.rotation.x + dq.x * 0.5f * dt,
					st.rotation.y + dq.y * 0.5f * dt, st.rotation.z + dq.z * 0.5f * dt));
			}
		}

		// めり込みの押し出し + 接地判定
		for (auto& m : manifolds)
		{
			if (m.trigger) { continue; }
			Rigidbody* ra = m.a->body;
			Rigidbody* rbB = m.b->body;
			float invMA = ra ? ra->GetInverseMass() : 0.0f;
			float invMB = rbB ? rbB->GetInverseMass() : 0.0f;
			float total = invMA + invMB;
			const ContactPoint* deepest = nullptr;
			for (auto& c : m.contacts)
			{
				if (!deepest || c.depth > deepest->depth) { deepest = &c; }
				if (ra && -c.normal.y > kGroundCos) { ra->mGrounded = true; ra->mGroundNormal = -c.normal; }
				if (rbB && c.normal.y > kGroundCos) { rbB->mGrounded = true; rbB->mGroundNormal = c.normal; }
			}
			if (!deepest || total <= 0.0f) { continue; }
			float corr = std::max(deepest->depth - kPenetrationSlop, 0.0f) * kCorrectionPercent / total;
			if (ra && ra->IsDynamic()) { states[ra].position -= deepest->normal * corr * invMA; }
			if (rbB && rbB->IsDynamic()) { states[rbB].position += deepest->normal * corr * invMB; }
		}

		// トランスフォームへ書き戻す
		for (auto& [rb, st] : states)
		{
			if (!rb->IsDynamic()) { continue; }
			Transform& t = rb->GetTransform();
			t.SetPosition(st.position);
			if (!rb->freezeRotation) { t.SetRotation(st.rotation); }
		}

		// ---------------- 6. イベント ----------------
		DispatchEvents(manifolds);
	}

	void PhysicsWorld::DispatchEvents(std::vector<Manifold>& manifolds)
	{
		std::map<PairKey, bool> current;
		auto send = [](Actor* actor, auto&& fn)
			{
				for (auto& c : actor->GetAllComponents()) { fn(c.get()); }
			};

		for (auto& m : manifolds)
		{
			Collider* ca = m.a->collider;
			Collider* cb = m.b->collider;
			PairKey key = ca < cb ? PairKey(ca, cb) : PairKey(cb, ca);
			current[key] = m.trigger;
			bool existed = mPrevPairs.contains(key);
			Actor* aa = ca->GetActor();
			Actor* ab = cb->GetActor();

			if (m.trigger)
			{
				if (existed)
				{
					send(aa, [&](Component* c) { c->OnTriggerStay(cb); });
					send(ab, [&](Component* c) { c->OnTriggerStay(ca); });
				}
				else
				{
					send(aa, [&](Component* c) { c->OnTriggerEnter(cb); });
					send(ab, [&](Component* c) { c->OnTriggerEnter(ca); });
				}
				continue;
			}

			// 代表点（最も深い接触）
			const ContactPoint* deepest = &m.contacts.front();
			for (auto& c : m.contacts) { if (c.depth > deepest->depth) { deepest = &c; } }
			Vector3 va = m.a->body ? m.a->body->velocity : Vector3::kZero;
			Vector3 vb = m.b->body ? m.b->body->velocity : Vector3::kZero;
			Collision colA{ ca, cb, ab, deepest->point, -deepest->normal, deepest->depth, va - vb };
			Collision colB{ cb, ca, aa, deepest->point, deepest->normal, deepest->depth, vb - va };
			if (existed)
			{
				send(aa, [&](Component* c) { c->OnCollisionStay(colA); });
				send(ab, [&](Component* c) { c->OnCollisionStay(colB); });
			}
			else
			{
				send(aa, [&](Component* c) { c->OnCollisionEnter(colA); });
				send(ab, [&](Component* c) { c->OnCollisionEnter(colB); });
			}
		}

		// 離れたペア
		for (auto& [key, trigger] : mPrevPairs)
		{
			if (current.contains(key)) { continue; }
			Collider* ca = key.first;
			Collider* cb = key.second;
			if (trigger)
			{
				send(ca->GetActor(), [&](Component* c) { c->OnTriggerExit(cb); });
				send(cb->GetActor(), [&](Component* c) { c->OnTriggerExit(ca); });
			}
			else
			{
				Collision colA{ ca, cb, cb->GetActor() };
				Collision colB{ cb, ca, ca->GetActor() };
				send(ca->GetActor(), [&](Component* c) { c->OnCollisionExit(colA); });
				send(cb->GetActor(), [&](Component* c) { c->OnCollisionExit(colB); });
			}
		}
		mPrevPairs = std::move(current);
	}

	bool PhysicsWorld::Raycast(const Ray& ray, float maxDistance, RaycastHit& hit, uint32_t layerMask, bool hitTriggers)
	{
		bool found = false;
		float best = maxDistance;
		for (Collider* c : mColliders)
		{
			if (!c->IsActiveAndEnabled()) { continue; }
			if (c->isTrigger && !hitTriggers) { continue; }
			if (!((layerMask >> c->GetActor()->GetLayer()) & 1u)) { continue; }
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
			if (!c->IsActiveAndEnabled() || !((layerMask >> c->GetActor()->GetLayer()) & 1u)) { continue; }
			WorldShape shape;
			if (!c->UpdateShape(shape)) { continue; }
			tmp.clear();
			if (CollideShapes(probe, shape, tmp)) { result.push_back(c); }
		}
		return result;
	}
}
