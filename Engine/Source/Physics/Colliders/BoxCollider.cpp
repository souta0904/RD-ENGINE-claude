#include "RdPch.h"
#include "Physics/Colliders/BoxCollider.h"
#include "Scene/Actor/Actor.h"

namespace rd
{
	void BoxCollider::Reflect(PropertyVisitor& v)
	{
		Collider::Reflect(v);
		v.Property("Size", size, { .speed = 0.01f, .min = 0.0f, .max = 10000.0f });
	}

	// ワールド行列の各軸の長さが拡大率、向きが箱の軸になる
	bool BoxCollider::UpdateShape(WorldShape& out)
	{
		const Matrix4& m = GetTransform().GetWorldMatrix();
		Vector3 axes[3] = { m.GetAxisX(), m.GetAxisY(), m.GetAxisZ() };
		out.type = ShapeType::Box;
		out.center = GetWorldCenter();
		for (int i = 0; i < 3; ++i)
		{
			float len = Length(axes[i]);
			out.axes[i] = len > 1.0e-6f ? axes[i] / len : Vector3::kZero;
			out.halfExtents[i] = size[i] * 0.5f * len;
		}
		out.ComputeBounds();
		return true;
	}
}
