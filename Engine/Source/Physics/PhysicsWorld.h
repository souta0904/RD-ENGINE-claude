#pragma once
#include "Physics/Shapes.h"
#include <map>
#include <vector>

namespace rd
{
	class Collider;
	class Rigidbody;
	class Scene;

	struct RaycastHit
	{
		Collider* collider = nullptr;
		Vector3 point;
		Vector3 normal;
		float distance = 0.0f;
	};

	// 物理ワールド
	//  1. 力の積分 2. ブロードフェーズ（X 軸のスイープ&プルーン）3. 狭域判定
	//  4. 逐次インパルス法で速度を解く 5. 位置の積分と押し出し 6. 接触イベントの通知
	class PhysicsWorld
	{
	public:
		void Register(Collider* collider);
		void Unregister(Collider* collider);

		void Step(Scene& scene, float dt);

		bool Raycast(const Ray& ray, float maxDistance, RaycastHit& hit, uint32_t layerMask = ~0u, bool hitTriggers = false);
		std::vector<Collider*> OverlapSphere(const Vector3& center, float radius, uint32_t layerMask = ~0u);

		Vector3 gravity = { 0.0f, -9.81f, 0.0f };
		int solverIterations = 10;

		// レイヤー同士が衝突するか
		void SetLayerCollision(uint32_t a, uint32_t b, bool collide);
		bool GetLayerCollision(uint32_t a, uint32_t b) const { return (mLayerMask[a & 31] >> (b & 31)) & 1u; }

		uint32_t GetBodyCount() const { return uint32_t(mColliders.size()); }
		uint32_t GetContactCount() const { return mLastContactCount; }

	private:
		struct Entry
		{
			Collider* collider = nullptr;
			Rigidbody* body = nullptr;// 動く剛体（なければ静的）
			WorldShape shape;
			uint32_t layer = 0;
			bool valid = false;
		};
		struct Manifold
		{
			Entry* a = nullptr;
			Entry* b = nullptr;
			std::vector<ContactPoint> contacts;
			std::vector<float> normalImpulse;
			std::vector<float> tangentImpulse;
			bool trigger = false;
		};
		using PairKey = std::pair<Collider*, Collider*>;

		void BuildEntries();
		void Solve(std::vector<Manifold>& manifolds, float dt);
		void DispatchEvents(std::vector<Manifold>& manifolds);

		std::vector<Collider*> mColliders;
		std::vector<Entry> mEntries;
		std::map<PairKey, bool> mPrevPairs;// 前ステップで接触していたペア（値は trigger か）
		uint32_t mLayerMask[32];
		uint32_t mLastContactCount = 0;

	public:
		PhysicsWorld();
	};
}
