#include "RdPch.h"
#include "Physics/World/PhysicsWorld.h"
#include "Physics/Colliders/Collider.h"
#include "Physics/Rigidbody/Rigidbody.h"
#include "Scene/Actor/Actor.h"

namespace rd
{
	namespace
	{
		void SendTriggerEnter(Actor* actor, Collider* other) { for (auto& c : actor->GetAllComponents()) { c->OnTriggerEnter(other); } }
		void SendTriggerStay(Actor* actor, Collider* other) { for (auto& c : actor->GetAllComponents()) { c->OnTriggerStay(other); } }
		void SendTriggerExit(Actor* actor, Collider* other) { for (auto& c : actor->GetAllComponents()) { c->OnTriggerExit(other); } }
		void SendCollisionEnter(Actor* actor, const Collision& col) { for (auto& c : actor->GetAllComponents()) { c->OnCollisionEnter(col); } }
		void SendCollisionStay(Actor* actor, const Collision& col) { for (auto& c : actor->GetAllComponents()) { c->OnCollisionStay(col); } }
		void SendCollisionExit(Actor* actor, const Collision& col) { for (auto& c : actor->GetAllComponents()) { c->OnCollisionExit(col); } }
	}

	// 前のステップに無かったペアは Enter、あったペアは Stay、今回無くなったペアは Exit を両方のアクターへ送る
	//  衝突の情報は一番深い接触点を代表にする（法線は自分を押し返す向き）
	void PhysicsWorld::DispatchEvents(std::vector<Manifold>& manifolds)
	{
		std::map<PairKey, bool> current;
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
				if (existed) { SendTriggerStay(aa, cb); SendTriggerStay(ab, ca); }
				else { SendTriggerEnter(aa, cb); SendTriggerEnter(ab, ca); }
				continue;
			}

			const ContactPoint* deepest = &m.contacts.front();
			for (auto& c : m.contacts) { if (c.depth > deepest->depth) { deepest = &c; } }
			Vector3 va = m.a->body ? m.a->body->velocity : Vector3::kZero;
			Vector3 vb = m.b->body ? m.b->body->velocity : Vector3::kZero;
			Collision colA{ ca, cb, ab, deepest->point, -deepest->normal, deepest->depth, va - vb };
			Collision colB{ cb, ca, aa, deepest->point, deepest->normal, deepest->depth, vb - va };
			if (existed) { SendCollisionStay(aa, colA); SendCollisionStay(ab, colB); }
			else { SendCollisionEnter(aa, colA); SendCollisionEnter(ab, colB); }
		}

		for (auto& [key, trigger] : mPrevPairs)
		{
			if (current.contains(key)) { continue; }
			Collider* ca = key.first;
			Collider* cb = key.second;
			if (trigger)
			{
				SendTriggerExit(ca->GetActor(), cb);
				SendTriggerExit(cb->GetActor(), ca);
			}
			else
			{
				Collision colA{ ca, cb, cb->GetActor() };
				Collision colB{ cb, ca, ca->GetActor() };
				SendCollisionExit(ca->GetActor(), colA);
				SendCollisionExit(cb->GetActor(), colB);
			}
		}
		mPrevPairs = std::move(current);
	}
}
