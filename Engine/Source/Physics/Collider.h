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
#if RD_EDITOR
		void OnDrawGizmos(DebugDraw& draw, bool selected) override;
#endif

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

	// 三角形メッシュのコライダー
	//  静的（地形・建物）: 三角形の面そのもので当たる。凹んだ形もそのまま使える
	//  動く物体（動的な Rigidbody 付き）または convex: 凸包として当たる（Unity の Convex と同じ考え方）
	//  凸包でないメッシュ同士は「頂点が相手の面より内側に入ったか」で調べるので、辺と辺だけの交差は拾えない。
	class MeshCollider final : public Collider
	{
		RD_COMPONENT(MeshCollider)
	public:
		std::string modelPath;// 空なら同じアクターの MeshRenderer のモデル
		bool convex = false;// 凸包として扱う（動的な Rigidbody が付いていれば常に凸包）
		void Reflect(PropertyVisitor& v) override;
		bool UpdateShape(WorldShape& out) override;
		Vector3 GetApproxSize() const override;

	private:
		AssetRef<Model> mModel;
		MeshCollisionData mData;// ローカル座標（center と拡大縮小は焼き込み済み）
		Vector3 mBuiltScale;
		Vector3 mBuiltCenter;
		const Model* mBuiltModel = nullptr;
	};
}
