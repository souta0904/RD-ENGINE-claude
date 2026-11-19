#pragma once
#include "Asset/Animation/AnimationClip.h"
#include "Asset/Asset.h"
#include "Asset/Model/MaterialDesc.h"
#include "Asset/Model/MeshVertex.h"
#include "Asset/Model/SubMesh.h"
#include "RHI/D3D12Common.h"
#include <memory>
#include <vector>

namespace rd
{
	struct ModelData;

	// 3D モデル（独自形式の .rdmodel と、assimp で obj / gltf / fbx などを読む。キーの末尾が "#anim" ならアニメーションだけを読む）
	class Model final : public Asset
	{
	public:
		// assimp で読む元のファイルの拡張子か（ext は小文字で "." 付き）
		static bool IsSourceExtension(const std::string& ext);
		// モデルとして読める拡張子か（.rdmodel を含む）
		static bool IsSupportedExtension(const std::string& ext);

		const char* GetTypeName() const override { return "Model"; }

		const std::vector<SubMesh>& GetSubMeshes() const { return mSubMeshes; }
		const std::vector<MaterialDesc>& GetMaterials() const { return mMaterials; }
		const std::shared_ptr<Skeleton>& GetSkeleton() const { return mSkeleton; }
		const std::vector<std::shared_ptr<AnimationClip>>& GetClips() const { return mClips; }
		std::shared_ptr<AnimationClip> FindClip(const std::string& name) const;
		const AABB& GetBounds() const { return mBounds; }
		bool HasSkinning() const { return mHasSkinning; }

		const D3D12_VERTEX_BUFFER_VIEW& GetVertexView() const { return mVertexView; }
		const D3D12_VERTEX_BUFFER_VIEW& GetSkinView() const { return mSkinView; }
		const D3D12_INDEX_BUFFER_VIEW& GetIndexView() const { return mIndexView; }

		// 衝突判定用（静的メッシュはノードの変換を焼き込み済み）
		const std::vector<Vector3>& GetCollisionPositions() const { return mCollisionPositions; }
		const std::vector<uint32_t>& GetCollisionIndices() const { return mCollisionIndices; }

		// プリミティブなど、手で作った頂点からモデルを作る
		void BuildFromData(std::vector<MeshVertex> vertices, std::vector<uint32_t> indices);

	protected:
		void Load() override;

	private:
		bool LoadBuiltin();
		void RegisterEmbeddedTextures(ModelData& data);
		void CreateGpuBuffers(const std::vector<MeshVertex>& vertices, const std::vector<SkinVertex>& skins,
			const std::vector<uint32_t>& indices);

		std::vector<SubMesh> mSubMeshes;
		std::vector<MaterialDesc> mMaterials;
		std::shared_ptr<Skeleton> mSkeleton;
		std::vector<std::shared_ptr<AnimationClip>> mClips;
		AABB mBounds;
		bool mHasSkinning = false;

		ComPtr<ID3D12Resource> mVertexBuffer;
		ComPtr<ID3D12Resource> mSkinBuffer;
		ComPtr<ID3D12Resource> mIndexBuffer;
		D3D12_VERTEX_BUFFER_VIEW mVertexView = {};
		D3D12_VERTEX_BUFFER_VIEW mSkinView = {};
		D3D12_INDEX_BUFFER_VIEW mIndexView = {};

		std::vector<Vector3> mCollisionPositions;
		std::vector<uint32_t> mCollisionIndices;
	};
}
