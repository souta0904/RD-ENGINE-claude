#pragma once
#include "Physics/Colliders/Collider.h"

namespace rd
{
	// カプセルのコライダー（height は両端の半球を含む長さ）
	class RD_API CapsuleCollider final : public Collider
	{
		RD_COMPONENT(CapsuleCollider)
	public:
		enum class Axis { X, Y, Z };
		float radius = 0.5f;
		float height = 2.0f;
		Axis axis = Axis::Y;

		void Reflect(PropertyVisitor& v) override;
		bool UpdateShape(WorldShape& out) override;
		Vector3 GetApproxSize() const override;
	};
}
