#pragma once
#include "Math/Geometry/Ray.h"
#include "Physics/Collision/ContactPoint.h"
#include "Physics/Collision/WorldShape.h"
#include "Physics/World/RaycastHit.h"
#include "Scene/SceneSystem.h"
#include <map>
#include <unordered_map>
#include <vector>

namespace rd
{
	class Collider;
	class Rigidbody;

	// 物理ワールド（シーンのシステムとして動く。scene.GetSystem<PhysicsWorld>()、重力はシーンの設定を使う）
	//  1. 力の積分 2. 大まかな判定（X 軸で並べて重なる範囲だけ調べる）3. 細かい判定
	//  4. 逐次インパルス法で速度を解く 5. 位置の積分とめり込みの押し出し 6. 接触のイベントを送る
	//  実装は PhysicsWorld.cpp（流れと判定・問い合わせ）/ PhysicsSolver.cpp（4 と 5）/ PhysicsEvents.cpp（6）
	class PhysicsWorld final : public SceneSystem
	{
	public:
		PhysicsWorld();

		void Register(Collider* collider);
		void Unregister(Collider* collider);

		void FixedUpdate(Scene& scene, float dt) override { Step(scene, dt); }
		void Step(Scene& scene, float dt);

		bool Raycast(const Ray& ray, float maxDistance, RaycastHit& hit, uint32_t layerMask = ~0u, bool hitTriggers = false);
		std::vector<Collider*> OverlapSphere(const Vector3& center, float radius, uint32_t layerMask = ~0u);
		// 範囲と重なるコライダーの形を集める（トリガーは除く。キャラクターの移動で使う）
		struct ShapeQuery
		{
			Collider* collider = nullptr;
			WorldShape shape;
		};
		void QueryShapes(const AABB& area, uint32_t layerMask, std::vector<ShapeQuery>& out);

		int solverIterations = 10;

		void SetLayerCollision(uint32_t a, uint32_t b, bool collide);
		bool GetLayerCollision(uint32_t a, uint32_t b) const { return (mLayerMask[a & 31] >> (b & 31)) & 1u; }

		uint32_t GetBodyCount() const { return uint32_t(mColliders.size()); }
		uint32_t GetContactCount() const { return mLastContactCount; }

	private:
		// body は動く剛体（なければ静的）
		struct Entry
		{
			Collider* collider = nullptr;
			Rigidbody* body = nullptr;
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
		// 接触点ごとに先に計算しておく値（r は重心から接触点、t1 / t2 は摩擦の向き、k は有効質量、bounce は跳ね返りの目標速度）
		struct ContactCache
		{
			Vector3 rA, rB, t1, t2;
			float bounce, kn, kt1, kt2, mu;
		};
		struct BodyState
		{
			Vector3 position;
			Quaternion rotation;
		};
		using PairKey = std::pair<Collider*, Collider*>;

		void IntegrateForces(Scene& scene, float dt);
		void BuildEntries();
		void FindContacts(std::vector<Manifold>& manifolds);
		void SolveVelocities(std::vector<Manifold>& manifolds);
		void PrepareContacts(std::vector<Manifold>& manifolds, std::vector<std::vector<ContactCache>>& caches);
		void IntegratePositions(float dt);
		void ResolvePenetration(std::vector<Manifold>& manifolds);
		void WriteBackTransforms();
		void DispatchEvents(std::vector<Manifold>& manifolds);

		Vector3 PositionOf(Rigidbody* rb) const;
		Vector3 ApplyInvInertia(Rigidbody* rb, const Vector3& v) const;
		void ApplyImpulse(Rigidbody* rb, const Vector3& r, const Vector3& impulse) const;
		float EffectiveMass(Rigidbody* ra, Rigidbody* rb, const Vector3& rA, const Vector3& rB, const Vector3& dir) const;

		std::vector<Collider*> mColliders;
		std::vector<Entry> mEntries;
		std::unordered_map<Rigidbody*, BodyState> mStates;
		// 前のステップで接していたペア（値は trigger か）
		std::map<PairKey, bool> mPrevPairs;
		uint32_t mLayerMask[32];
		uint32_t mLastContactCount = 0;
	};
}
