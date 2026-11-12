#pragma once
#include "Asset/Asset.h"
#include "Physics/Shapes.h"
#include "Scene/Component.h"
#include <memory>

namespace rd
{
	class Model;
	class Rigidbody;

	// 衝突情報（OnCollisionXxx に渡される）
	struct Collision
	{
		Collider* collider = nullptr;// 自分のコライダー
		Collider* otherCollider = nullptr;
		Actor* other = nullptr;
		Vector3 point;
		Vector3 normal;// 相手から自分へ向かう向き（自分を押し返す向き）
		float depth = 0.0f;
		Vector3 relativeVelocity;
	};

	// コライダー基底
	class Collider : public Component
	{
	public:
		Vector3 center;// ローカルのオフセット
		bool isTrigger = false;
		float friction = 0.5f;
		float restitution = 0.0f;

		void Reflect(PropertyVisitor& v) override;
		void Awake() override;
		void OnDestroy() override;
		void OnDrawGizmos(DebugDraw& draw, bool selected) override;

		// ワールド形状を更新して返す
		virtual bool UpdateShape(WorldShape& out) = 0;
		Rigidbody* GetAttachedRigidbody() const;
		// 慣性テンソル計算用のローカル半径（おおよそ）
		virtual Vector3 GetApproxSize() const = 0;

	protected:
		Vector3 GetWorldCenter() const;
		Vector3 GetAbsScale() const;
	};

	class SphereCollider final : public Collider
	{
		RD_COMPONENT(SphereCollider)
	public:
		float radius = 0.5f;
		void Reflect(PropertyVisitor& v) override;
		bool UpdateShape(WorldShape& out) override;
		Vector3 GetApproxSize() const override { return Vector3(radius, radius, radius) * 2.0f; }
	};

	class BoxCollider final : public Collider
	{
		RD_COMPONENT(BoxCollider)
	public:
		Vector3 size = Vector3::kOne;
		void Reflect(PropertyVisitor& v) override;
		bool UpdateShape(WorldShape& out) override;
		Vector3 GetApproxSize() const override { return size; }
	};

	class CapsuleCollider final : public Collider
	{
		RD_COMPONENT(CapsuleCollider)
	public:
		enum class Axis { X, Y, Z };
		float radius = 0.5f;
		float height = 2.0f;// 両端の半球を含む長さ
		Axis axis = Axis::Y;
		void Reflect(PropertyVisitor& v) override;
		bool UpdateShape(WorldShape& out) override;
		Vector3 GetApproxSize() const override;
	};

	// 静的な三角形メッシュ（地形など）。動くものには使えない
	class MeshCollider final : public Collider
	{
		RD_COMPONENT(MeshCollider)
	public:
		std::string modelPath;// 空なら同じアクターの MeshRenderer のモデル
		void Reflect(PropertyVisitor& v) override;
		bool UpdateShape(WorldShape& out) override;
		Vector3 GetApproxSize() const override { return Vector3::kOne; }

	private:
		AssetRef<Model> mModel;
		MeshCollisionData mData;
		Matrix4 mBuiltMatrix;
		const Model* mBuiltModel = nullptr;
	};
}
