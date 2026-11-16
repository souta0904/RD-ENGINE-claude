#include "RdPch.h"
#include "Physics/Colliders/MeshCollider.h"
#include "Scene/Actor/Actor.h"
#include "Asset/AssetManager.h"
#include "Asset/Model/Model.h"
#include "Components/Rendering/MeshRenderer.h"
#include "Physics/Rigidbody/Rigidbody.h"

namespace rd
{
	void MeshCollider::Reflect(PropertyVisitor& v)
	{
		Collider::Reflect(v);
		v.Property("Model", modelPath, { .flags = PropertyMeta::AssetModel, .tooltip = "Empty = use MeshRenderer's model" });
		v.Property("Convex", convex, { .tooltip = "Collide as the convex hull. Always on when a dynamic Rigidbody is attached" });
	}

	const Model* MeshCollider::FindModel()
	{
		if (!modelPath.empty())
		{
			if (!mModel || mModel->GetPath() != modelPath) { mModel = AssetManager::Get().LoadModel(modelPath); }
			return mModel.get();
		}
		if (auto mr = GetComponent<MeshRenderer>()) { return mr->GetModel().get(); }
		return nullptr;
	}

	// 三角形はローカル座標（ずれと拡大率を焼き込む）で持つので、作り直すのはモデルか拡大率が変わったときだけ。
	// 移動と回転は形の変換として渡す（動く物体にも使える）
	// 動く物体は凸包として扱う（三角形の面のままだと、深くめり込んだときに押し返す向きが決まらない）
	bool MeshCollider::UpdateShape(WorldShape& out)
	{
		const Model* model = FindModel();
		if (!model || !model->IsReady() || model->GetCollisionIndices().empty()) { return false; }

		const Matrix4& world = GetTransform().GetWorldMatrix();
		Vector3 scale;
		Quaternion rotation;
		Vector3 position;
		world.Decompose(scale, rotation, position);
		if (model != mBuiltModel || LengthSq(scale - mBuiltScale) > 1.0e-10f || LengthSq(center - mBuiltCenter) > 1.0e-10f)
		{
			std::vector<Vector3> positions;
			positions.reserve(model->GetCollisionPositions().size());
			for (const auto& p : model->GetCollisionPositions()) { positions.push_back((p + center) * scale); }
			mData.Build(std::move(positions), model->GetCollisionIndices());
			mBuiltModel = model;
			mBuiltScale = scale;
			mBuiltCenter = center;
		}
		out.type = ShapeType::Mesh;
		out.mesh = &mData;
		out.meshRotation = rotation;
		out.meshPosition = position;
		Rigidbody* rb = GetAttachedRigidbody();
		out.meshConvex = convex || (rb && rb->IsDynamic());
		out.center = position;
		out.ComputeBounds();
		return true;
	}

	// まだ作っていなければ単位の箱とみなす
	Vector3 MeshCollider::GetApproxSize() const
	{
		return mBuiltModel ? mData.GetBounds().GetExtents() * 2.0f : Vector3::kOne;
	}
}
