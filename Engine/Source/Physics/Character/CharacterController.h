#pragma once
#include "Physics/Colliders/Collider.h"
#include "Physics/World/PhysicsWorld.h"
#include <vector>

namespace rd
{
	// Move で当たった面（normal は相手からキャラクターへ向かう向き）
	struct ControllerHit
	{
		Collider* collider = nullptr;
		Vector3 point;
		Vector3 normal;
	};

	// Move の結果（どこが当たったか）
	enum CollisionFlags : uint32_t
	{
		kCollisionNone = 0,
		kCollisionSides = 1,
		kCollisionAbove = 2,
		kCollisionBelow = 4,
	};

	// Unity の CharacterController と同じ使い方のキャラクター移動
	//  剛体を使わず、Move で動かしたあとに重なった分だけ押し戻す（壁は面の向きへ戻すので沿って滑り、
	//  床は真上へ戻すので坂で滑り落ちない）。速い移動は半径の半分ずつに分けてすり抜けを防ぐ。
	//  形は常に縦向きのカプセル（半径は X と Z の拡大率の大きい方、高さは Y の拡大率で伸ばす）。
	//  ほかからはカプセルのコライダーとして見えるので、レイやトリガーに当たる。
	class CharacterController final : public Collider
	{
		RD_COMPONENT(CharacterController)
	public:
		float radius = 0.5f;
		float height = 2.0f;
		float slopeLimit = 45.0f;
		float stepOffset = 0.3f;
		float skinWidth = 0.08f;

		void Reflect(PropertyVisitor& v) override;
		bool UpdateShape(WorldShape& out) override;
		Vector3 GetApproxSize() const override;

		uint32_t Move(const Vector3& motion);
		bool IsGrounded() const { return mGrounded; }
		const Vector3& GetVelocity() const { return mVelocity; }
		const std::vector<ControllerHit>& GetHits() const { return mHits; }

	private:
		WorldShape MakeShape(const Vector3& position) const;
		bool IsOwnCollider(const Collider* c) const;
		uint32_t PushOut(Vector3& position);

		std::vector<PhysicsWorld::ShapeQuery> mShapes;
		std::vector<ControllerHit> mHits;
		Vector3 mVelocity;
		bool mGrounded = false;
	};
}
