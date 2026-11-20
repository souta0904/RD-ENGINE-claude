#pragma once
#include "Physics/Collision/WorldShape.h"
#include "Physics/Colliders/Collision.h"
#include "Scene/Component/Component.h"

namespace rd
{
	class Rigidbody;

	// コライダーの基底（center はローカルのずれ）
	class RD_API Collider : public Component
	{
	public:
		Vector3 center;
		bool isTrigger = false;
		float friction = 0.5f;
		float restitution = 0.0f;

		void Reflect(PropertyVisitor& v) override;
		void Awake() override;
		void OnDestroy() override;
#if RD_EDITOR
		void OnDrawGizmos(DebugDraw& draw, bool selected) override;
#endif

		// ワールドでの形を求める（形を作れなければ false）
		virtual bool UpdateShape(WorldShape& out) = 0;
		// 同じアクターか親に付いている剛体
		Rigidbody* GetAttachedRigidbody() const;
		// 慣性テンソルを求めるためのおおよその大きさ
		virtual Vector3 GetApproxSize() const = 0;

	protected:
		Vector3 GetWorldCenter() const;
		Vector3 GetAbsScale() const;
	};
}
