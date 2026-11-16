#pragma once
#include "Asset/Asset.h"
#include "Physics/Colliders/Collider.h"

namespace rd
{
	class Model;

	// 三角形メッシュのコライダー
	//  静的（地形・建物）: 三角形の面そのもので当たる。凹んだ形もそのまま使える
	//  動的な Rigidbody 付きか convex: 凸包として当たる（Unity の Convex と同じ考え方）
	//  凸包でないメッシュ同士は「頂点が相手の面より内側に入ったか」で調べるので、辺と辺だけの交差は拾えない
	//  modelPath が空なら同じアクターの MeshRenderer のモデルを使う。mData はローカル座標（center と拡大率は焼き込み済み）
	class MeshCollider final : public Collider
	{
		RD_COMPONENT(MeshCollider)
	public:
		std::string modelPath;
		bool convex = false;

		void Reflect(PropertyVisitor& v) override;
		bool UpdateShape(WorldShape& out) override;
		Vector3 GetApproxSize() const override;

	private:
		const Model* FindModel();

		AssetRef<Model> mModel;
		MeshCollisionData mData;
		Vector3 mBuiltScale;
		Vector3 mBuiltCenter;
		const Model* mBuiltModel = nullptr;
	};
}
