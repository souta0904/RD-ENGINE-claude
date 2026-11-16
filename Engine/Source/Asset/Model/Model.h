#pragma once
#include "Asset/Animation/AnimationClip.h"
#include "Asset/Asset.h"
#include "Asset/Model/MaterialDesc.h"
#include "Asset/Model/MeshVertex.h"
#include "Asset/Model/SubMesh.h"
#include "RHI/D3D12Common.h"
#include <memory>
#include <vector>

struct aiScene;
struct aiNode;

namespace rd
{
	// 3D モデル（assimp で obj / gltf / fbx などを読む。キーの末尾が "#anim" ならアニメーションだけを読む）
	class Model final : public Asset
	{
	public:
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
		struct MeshData
		{
			std::vector<MeshVertex> vertices;
			std::vector<SkinVertex> skins;
			std::vector<uint32_t> indices;
		};

		bool LoadBuiltin();
		void ImportSkeleton(const aiNode* node, int32_t parent);
		void ImportAnimations(const aiScene* scene);
		void ImportMaterials(const aiScene* scene, const std::string& path);
		std::string ImportTexture(const aiScene* scene, const std::string& dir, const std::string& texPath, bool srgb);
		void ImportMeshes(const aiScene* scene, const aiNode* node, const Matrix4& parentGlobal, MeshData& out);
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
