#include "RdPch.h"
#include "Physics/World/PhysicsWorld.h"
#include "Physics/Colliders/Collider.h"
#include "Physics/Rigidbody/Rigidbody.h"
#include "Scene/Actor/Actor.h"

namespace rd
{
	namespace
	{
		constexpr float kPenetrationSlop = 0.005f;
		constexpr float kCorrectionPercent = 0.6f;
		// これより遅い衝突では跳ね返らない
		constexpr float kRestitutionThreshold = 1.0f;
		// これより上を向いた面を地面とみなす
		constexpr float kGroundCos = 0.7f;

		Vector3 VelocityAt(Rigidbody* rb, const Vector3& r)
		{
			return rb ? rb->velocity + Cross(rb->angularVelocity, r) : Vector3::kZero;
		}
	}

	Vector3 PhysicsWorld::PositionOf(Rigidbody* rb) const
	{
		auto it = mStates.find(rb);
		return it != mStates.end() ? it->second.position : Vector3::kZero;
	}

	// ワールドのベクトルをローカルへ回し、ローカルの逆慣性テンソル（対角）を掛けて戻す
	Vector3 PhysicsWorld::ApplyInvInertia(Rigidbody* rb, const Vector3& v) const
	{
		if (!rb || !rb->IsDynamic()) { return Vector3::kZero; }
		auto it = mStates.find(rb);
		Quaternion q = it != mStates.end() ? it->second.rotation : Quaternion::kIdentity;
		return ((v * Conjugate(q)) * rb->mInvInertiaLocal) * q;
	}

	void PhysicsWorld::ApplyImpulse(Rigidbody* rb, const Vector3& r, const Vector3& impulse) const
	{
		if (!rb || !rb->IsDynamic()) { return; }
		rb->velocity += impulse * rb->GetInverseMass();
		rb->angularVelocity += ApplyInvInertia(rb, Cross(r, impulse));
	}

	// dir の向きに力を加えたときの有効質量（質量と回転のしにくさを合わせたもの）
	float PhysicsWorld::EffectiveMass(Rigidbody* ra, Rigidbody* rb, const Vector3& rA, const Vector3& rB, const Vector3& dir) const
	{
		float k = (ra ? ra->GetInverseMass() : 0.0f) + (rb ? rb->GetInverseMass() : 0.0f);
		k += Dot(dir, Cross(ApplyInvInertia(ra, Cross(rA, dir)), rA));
		k += Dot(dir, Cross(ApplyInvInertia(rb, Cross(rB, dir)), rB));
		return k > 1.0e-8f ? 1.0f / k : 0.0f;
	}

	// 接触点ごとに、有効質量と摩擦の向き、跳ね返りの目標速度を先に求めておく
	void PhysicsWorld::PrepareContacts(std::vector<Manifold>& manifolds, std::vector<std::vector<ContactCache>>& caches)
	{
		caches.resize(manifolds.size());
		for (size_t mi = 0; mi < manifolds.size(); ++mi)
		{
			Manifold& m = manifolds[mi];
			if (m.trigger) { continue; }
			Rigidbody* ra = m.a->body;
			Rigidbody* rb = m.b->body;
			float mu = std::sqrt(m.a->collider->friction * m.b->collider->friction);
			float e = std::max(m.a->collider->restitution, m.b->collider->restitution);
			for (auto& c : m.contacts)
			{
				ContactCache cc;
				cc.rA = ra ? c.point - PositionOf(ra) : Vector3::kZero;
				cc.rB = rb ? c.point - PositionOf(rb) : Vector3::kZero;
				const Vector3& n = c.normal;
				cc.kn = EffectiveMass(ra, rb, cc.rA, cc.rB, n);
				cc.t1 = Normalize(Cross(std::fabs(n.y) < 0.9f ? Vector3::kUnitY : Vector3::kUnitX, n));
				cc.t2 = Cross(n, cc.t1);
				cc.kt1 = EffectiveMass(ra, rb, cc.rA, cc.rB, cc.t1);
				cc.kt2 = EffectiveMass(ra, rb, cc.rA, cc.rB, cc.t2);
				cc.mu = mu;
				float vn = Dot(VelocityAt(rb, cc.rB) - VelocityAt(ra, cc.rA), n);
				cc.bounce = vn < -kRestitutionThreshold ? -e * vn : 0.0f;
				caches[mi].push_back(cc);
			}
		}
	}

	// 逐次インパルス法: 接触点ごとに法線方向の力（押し返すだけ、引っ張らない）と、
	//  法線の力に比例した上限つきの摩擦（2 方向）を、積み上げた合計を範囲に収めながら何回か繰り返す
	void PhysicsWorld::SolveVelocities(std::vector<Manifold>& manifolds)
	{
		std::vector<std::vector<ContactCache>> caches;
		PrepareContacts(manifolds, caches);

		for (int iter = 0; iter < solverIterations; ++iter)
		{
			for (size_t mi = 0; mi < manifolds.size(); ++mi)
			{
				Manifold& m = manifolds[mi];
				if (m.trigger) { continue; }
				Rigidbody* ra = m.a->body;
				Rigidbody* rb = m.b->body;
				for (size_t ci = 0; ci < m.contacts.size(); ++ci)
				{
					const ContactCache& cc = caches[mi][ci];
					const Vector3& n = m.contacts[ci].normal;

					Vector3 vrel = VelocityAt(rb, cc.rB) - VelocityAt(ra, cc.rA);
					float dj = (cc.bounce - Dot(vrel, n)) * cc.kn;
					float old = m.normalImpulse[ci];
					m.normalImpulse[ci] = std::max(old + dj, 0.0f);
					dj = m.normalImpulse[ci] - old;
					ApplyImpulse(ra, cc.rA, -n * dj);
					ApplyImpulse(rb, cc.rB, n * dj);

					float maxF = cc.mu * m.normalImpulse[ci];
					const Vector3 tangents[2] = { cc.t1, cc.t2 };
					const float kts[2] = { cc.kt1, cc.kt2 };
					for (int k = 0; k < 2; ++k)
					{
						vrel = VelocityAt(rb, cc.rB) - VelocityAt(ra, cc.rA);
						float djt = -Dot(vrel, tangents[k]) * kts[k];
						float& acc = m.tangentImpulse[ci * 2 + size_t(k)];
						float prev = acc;
						acc = math::Clamp(prev + djt, -maxF, maxF);
						djt = acc - prev;
						ApplyImpulse(ra, cc.rA, -tangents[k] * djt);
						ApplyImpulse(rb, cc.rB, tangents[k] * djt);
					}
				}
			}
		}
	}

	// 角速度 w の回転は dq/dt = q * (0, w) / 2 として足し、正規化する
	void PhysicsWorld::IntegratePositions(float dt)
	{
		for (auto& [rb, st] : mStates)
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
	}

	// 一番深い接触点のめり込みの一部を、質量の逆数の比で押し出す。あわせて上向きの面に触れたかを記録する
	void PhysicsWorld::ResolvePenetration(std::vector<Manifold>& manifolds)
	{
		for (auto& m : manifolds)
		{
			if (m.trigger) { continue; }
			Rigidbody* ra = m.a->body;
			Rigidbody* rb = m.b->body;
			float invMA = ra ? ra->GetInverseMass() : 0.0f;
			float invMB = rb ? rb->GetInverseMass() : 0.0f;
			float total = invMA + invMB;
			const ContactPoint* deepest = nullptr;
			for (auto& c : m.contacts)
			{
				if (!deepest || c.depth > deepest->depth) { deepest = &c; }
				if (ra && -c.normal.y > kGroundCos) { ra->mGrounded = true; ra->mGroundNormal = -c.normal; }
				if (rb && c.normal.y > kGroundCos) { rb->mGrounded = true; rb->mGroundNormal = c.normal; }
			}
			if (!deepest || total <= 0.0f) { continue; }
			float corr = std::max(deepest->depth - kPenetrationSlop, 0.0f) * kCorrectionPercent / total;
			if (ra && ra->IsDynamic()) { mStates[ra].position -= deepest->normal * corr * invMA; }
			if (rb && rb->IsDynamic()) { mStates[rb].position += deepest->normal * corr * invMB; }
		}
	}

	void PhysicsWorld::WriteBackTransforms()
	{
		for (auto& [rb, st] : mStates)
		{
			if (!rb->IsDynamic()) { continue; }
			Transform& t = rb->GetTransform();
			t.SetPosition(st.position);
			if (!rb->freezeRotation) { t.SetRotation(st.rotation); }
		}
	}
}
