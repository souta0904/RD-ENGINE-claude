#pragma once
#include "Physics/Colliders/Collider.h"

namespace rd
{
	// 箱のコライダー（回転したアクターの向きに合わせた箱）
	class RD_API BoxCollider final : public Collider
	{
		RD_COMPONENT(BoxCollider)
	public:
		Vector3 size = Vector3::kOne;

		void Reflect(PropertyVisitor& v) override;
		bool UpdateShape(WorldShape& out) override;
		Vector3 GetApproxSize() const override { return size; }
	};
}
