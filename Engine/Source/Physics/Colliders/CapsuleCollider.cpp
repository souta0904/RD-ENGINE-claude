#include "RdPch.h"
#include "Physics/Colliders/CapsuleCollider.h"
#include "Scene/Actor/Actor.h"

namespace rd
{
	void CapsuleCollider::Reflect(PropertyVisitor& v)
	{
		Collider::Reflect(v);
		v.Property("Radius", radius, { .speed = 0.01f, .min = 0.0f, .max = 1000.0f });
		v.Property("Height", height, { .speed = 0.01f, .min = 0.0f, .max = 1000.0f });
		v.Property("Axis", axis);
	}

	Vector3 CapsuleCollider::GetApproxSize() const
	{
		Vector3 s(radius * 2.0f, radius * 2.0f, radius * 2.0f);
		s[int(axis)] = std::max(height, radius * 2.0f);
		return s;
	}

	// 半径は軸以外の拡大率の大きい方、長さは軸の拡大率で伸ばす
	bool CapsuleCollider::UpdateShape(WorldShape& out)
	{
		const Matrix4& m = GetTransform().GetWorldMatrix();
		Vector3 scale = GetAbsScale();
		int ax = int(axis);
		Vector3 dir = Normalize(ax == 0 ? m.GetAxisX() : (ax == 1 ? m.GetAxisY() : m.GetAxisZ()));
		float radiusScale = 0.0f;
		for (int i = 0; i < 3; ++i) { if (i != ax) { radiusScale = std::max(radiusScale, scale[i]); } }
		out.type = ShapeType::Capsule;
		out.center = GetWorldCenter();
		out.radius = radius * radiusScale;
		float half = std::max(0.0f, height * scale[ax] * 0.5f - out.radius);
		out.p0 = out.center - dir * half;
		out.p1 = out.center + dir * half;
		out.ComputeBounds();
		return true;
	}
}
