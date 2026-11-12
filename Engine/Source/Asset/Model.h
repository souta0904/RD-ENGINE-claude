#pragma once
#include "Asset/Animation.h"
#include "Asset/Asset.h"
#include "Math/Geometry.h"
#include "RHI/D3D12Common.h"
#include <memory>
#include <vector>

namespace rd
{
	class Texture;

	// 頂点形式
	struct MeshVertex
	{
		Vector3 position;
		Vector3 normal;
		Vector4 tangent;// w は従法線の向き
		Vector2 uv;
	};
	struct SkinVertex
	{
		uint32_t joints[4] = {};
		float weights[4] = {};
	};

	// モデル内マテリアルの初期値
	struct MaterialDesc
	{
		std::string name;
		Color baseColor = Color::kWhite;
		float metallic = 0.0f;
		float roughness = 0.6f;
		Color emissive = Color::kBlack;
		std::string albedoPath;
		std::string normalPath;
		bool transparent = false;
	};

	struct SubMesh
	{
		std::string name;
		uint32_t indexStart = 0;
		uint32_t indexCount = 0;
		uint32_t materialIndex = 0;
		bool skinned = false;
		AABB bounds;
	};

	// 3D モデル（assimp で obj / gltf / fbx などを読み込む）
	//  キーの末尾が "#anim" ならアニメーションだけを読み込む
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

		// 衝突判定用（静的メッシュはノード変換を適用済み）
		const std::vector<Vector3>& GetCollisionPositions() const { return mCollisionPositions; }
		const std::vector<uint32_t>& GetCollisionIndices() const { return mCollisionIndices; }

		// プリミティブ形状を生成（"Engine/Cube" などの組み込みモデル用）
		void BuildFromData(std::vector<MeshVertex> vertices, std::vector<uint32_t> indices);

	protected:
		void Load() override;

	private:
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

	// 組み込みプリミティブ（キー: "Engine/Cube", "Engine/Sphere", "Engine/Plane", "Engine/Capsule"）
	namespace Primitives
	{
		void Cube(std::vector<MeshVertex>& v, std::vector<uint32_t>& i);
		void Sphere(std::vector<MeshVertex>& v, std::vector<uint32_t>& i, uint32_t segments = 32, uint32_t rings = 16);
		void Plane(std::vector<MeshVertex>& v, std::vector<uint32_t>& i, float size = 10.0f);
		void Capsule(std::vector<MeshVertex>& v, std::vector<uint32_t>& i, float radius = 0.5f, float height = 2.0f);
	}
}
