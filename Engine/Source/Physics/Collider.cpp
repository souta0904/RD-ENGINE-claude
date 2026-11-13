#include "RdPch.h"
#include "Physics/Collider.h"
#include "Asset/AssetManager.h"
#include "Asset/Model.h"
#include "Components/MeshRenderer.h"
#include "Physics/PhysicsWorld.h"
#include "Physics/Rigidbody.h"
#include "Render/DebugDraw.h"
#include "Scene/Actor.h"
#include "Scene/Scene.h"

namespace rd
{
	// ==================================================
	// Collider
	// ==================================================
	void Collider::Reflect(PropertyVisitor& v)
	{
		v.Property("Center", center);
		v.Property("Is Trigger", isTrigger);
		v.Property("Friction", friction, { .speed = 0.01f, .min = 0.0f, .max = 2.0f });
		v.Property("Restitution", restitution, { .speed = 0.01f, .min = 0.0f, .max = 1.0f });
	}

	void Collider::Awake()
	{
		GetScene()->GetSystem<PhysicsWorld>().Register(this);
	}

	void Collider::OnDestroy()
	{
		GetScene()->GetSystem<PhysicsWorld>().Unregister(this);
	}

	Rigidbody* Collider::GetAttachedRigidbody() const
	{
		// 同じアクターか親に付いている剛体
		for (Actor* a = GetActor(); a; a = a->GetParent())
		{
			if (auto rb = a->GetComponent<Rigidbody>(); rb && rb->IsEnabled()) { return rb; }
		}
		return nullptr;
	}

	Vector3 Collider::GetWorldCenter() const
	{
		return TransformPoint(center, GetTransform().GetWorldMatrix());
	}

	Vector3 Collider::GetAbsScale() const
	{
		return Abs(GetTransform().GetLossyScale());
	}

#if RD_EDITOR
	void Collider::OnDrawGizmos(DebugDraw& draw, bool selected)
	{
		WorldShape s;
		if (!UpdateShape(s)) { return; }
		Color color = isTrigger ? Color(0.3f, 0.8f, 1.0f, selected ? 1.0f : 0.5f) : Color(0.3f, 1.0f, 0.4f, selected ? 1.0f : 0.5f);
		switch (s.type)
		{
		case ShapeType::Sphere:
			draw.Sphere(s.center, s.radius, color);
			break;
		case ShapeType::Box:
		{
			Matrix4 m(
				s.axes[0].x * s.halfExtents.x * 2, s.axes[0].y * s.halfExtents.x * 2, s.axes[0].z * s.halfExtents.x * 2, 0,
				s.axes[1].x * s.halfExtents.y * 2, s.axes[1].y * s.halfExtents.y * 2, s.axes[1].z * s.halfExtents.y * 2, 0,
				s.axes[2].x * s.halfExtents.z * 2, s.axes[2].y * s.halfExtents.z * 2, s.axes[2].z * s.halfExtents.z * 2, 0,
				s.center.x, s.center.y, s.center.z, 1);
			draw.Box(m, color);
			break;
		}
		case ShapeType::Capsule:
			draw.Capsule(s.p0, s.p1, s.radius, color);
			break;
		case ShapeType::Mesh:
			if (selected) { draw.Box(s.bounds, color); }
			break;
		}
	}
#endif

	// ==================================================
	// SphereCollider
	// ==================================================
	void SphereCollider::Reflect(PropertyVisitor& v)
	{
		Collider::Reflect(v);
		v.Property("Radius", radius, { .speed = 0.01f, .min = 0.0f, .max = 1000.0f });
	}

	bool SphereCollider::UpdateShape(WorldShape& out)
	{
		Vector3 s = GetAbsScale();
		out.type = ShapeType::Sphere;
		out.center = GetWorldCenter();
		out.radius = radius * std::max(s.x, std::max(s.y, s.z));
		out.ComputeBounds();
		return true;
	}

	// ==================================================
	// BoxCollider
	// ==================================================
	void BoxCollider::Reflect(PropertyVisitor& v)
	{
		Collider::Reflect(v);
		v.Property("Size", size, { .speed = 0.01f, .min = 0.0f, .max = 10000.0f });
	}

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

	// ==================================================
	// CapsuleCollider
	// ==================================================
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

	// ==================================================
	// MeshCollider
	// ==================================================
	void MeshCollider::Reflect(PropertyVisitor& v)
	{
		Collider::Reflect(v);
		v.Property("Model", modelPath, { .flags = PropertyMeta::AssetModel, .tooltip = "Empty = use MeshRenderer's model" });
	}

	bool MeshCollider::UpdateShape(WorldShape& out)
	{
		// 使うモデルを決める
		const Model* model = nullptr;
		if (!modelPath.empty())
		{
			if (!mModel || mModel->GetPath() != modelPath) { mModel = AssetManager::Get().LoadModel(modelPath); }
			model = mModel.get();
		}
		else if (auto mr = GetComponent<MeshRenderer>())
		{
			model = mr->GetModel().get();
		}
		if (!model || !model->IsReady() || model->GetCollisionIndices().empty()) { return false; }

		// 変換やモデルが変わったときだけ作り直す（静的メッシュ前提）
		const Matrix4& world = GetTransform().GetWorldMatrix();
		if (model != mBuiltModel || std::memcmp(&world, &mBuiltMatrix, sizeof(Matrix4)) != 0)
		{
			std::vector<Vector3> positions;
			positions.reserve(model->GetCollisionPositions().size());
			Matrix4 m = Matrix4::CreateTranslate(center) * world;
			for (const auto& p : model->GetCollisionPositions()) { positions.push_back(TransformPoint(p, m)); }
			mData.Build(std::move(positions), model->GetCollisionIndices());
			mBuiltModel = model;
			mBuiltMatrix = world;
		}
		out.type = ShapeType::Mesh;
		out.mesh = &mData;
		out.center = world.GetTranslation();
		out.ComputeBounds();
		return true;
	}
}
