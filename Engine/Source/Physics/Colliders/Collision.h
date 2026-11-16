#pragma once
#include "Math/Vector3.h"

namespace rd
{
	class Actor;
	class Collider;

	// 衝突の情報（OnCollisionXxx に渡す。normal は相手から自分へ向かう向き = 自分を押し返す向き）
	struct Collision
	{
		Collider* collider = nullptr;
		Collider* otherCollider = nullptr;
		Actor* other = nullptr;
		Vector3 point;
		Vector3 normal;
		float depth = 0.0f;
		Vector3 relativeVelocity;
	};
}
