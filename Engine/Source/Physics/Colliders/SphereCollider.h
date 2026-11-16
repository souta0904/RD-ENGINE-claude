#pragma once
#include "Physics/Colliders/Collider.h"

namespace rd
{
	// 球のコライダー（半径は一番大きい拡大率で広げる）
	class SphereCollider final : public Collider
	{
		RD_COMPONENT(SphereCollider)
	public:
		float radius = 0.5f;

		void Reflect(PropertyVisitor& v) override;
		bool UpdateShape(WorldShape& out) override;
		Vector3 GetApproxSize() const override { return Vector3(radius, radius, radius) * 2.0f; }
	};
}
