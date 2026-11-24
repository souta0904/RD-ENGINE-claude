#include "RdPch.h"
#include "Asset/Model/Model.h"
#include "Asset/AssetManager.h"
#include "Asset/Model/MeshUtil.h"
#include "Asset/Model/ModelData.h"
#include "Asset/Model/ModelFile.h"
#include "Asset/Model/ModelImporter.h"
#include "Asset/Model/Primitives.h"
#include "Asset/Texture/Texture.h"
#include "Core/Util/StringUtil.h"
#include "RHI/Device/GraphicsDevice.h"

namespace rd
{
	bool Model::IsSourceExtension(const std::string& ext)
	{
		return ext == ".obj" || ext == ".gltf" || ext == ".glb" || ext == ".fbx" || ext == ".dae";
	}

	bool Model::IsSupportedExtension(const std::string& ext)
	{
		return ext == ModelFile::kExtension || IsSourceExtension(ext);
	}

	std::shared_ptr<AnimationClip> Model::FindClip(const std::string& name) const
	{
		if (mClips.empty()) { return nullptr; }
		if (name.empty()) { return mClips.front(); }
		for (const auto& clip : mClips)
		{
			if (clip->name == name) { return clip; }
		}
		return nullptr;
	}

	// .rdmodel はそのまま読み、それ以外は assimp で解析する。どちらも ModelData になるので、後の処理は同じ
	void Model::Load()
	{
		if (mPath.starts_with("Engine/"))
		{
			if (!LoadBuiltin()) { SetState(AssetState::Failed); }
			return;
		}

		std::string path = mPath;
		bool animOnly = EndsWith(path, "#anim");
		if (animOnly) { path.resize(path.size() - std::strlen("#anim")); }

		ModelData data;
		std::string error;
		bool loaded = GetExtension(path) == ModelFile::kExtension
			? ModelFile::Read(path, animOnly, data, error)
			: ModelImporter::Import(path, animOnly, data, error);
		if (!loaded)
		{
			Logger::Error(LogCat::Asset, "Failed to load model '{}': {}", path, error);
			SetState(AssetState::Failed);
			return;
		}

		mSkeleton = std::move(data.skeleton);
		mClips = std::move(data.clips);
		if (animOnly)
		{
			SetState(AssetState::Ready);
			return;
		}

		RegisterEmbeddedTextures(data);
		mMaterials = std::move(data.materials);
		mSubMeshes = std::move(data.subMeshes);
		mBounds = data.bounds;
		mHasSkinning = data.hasSkinning;
		if (data.vertices.empty())
		{
			SetState(AssetState::Ready);
			return;
		}

		mCollisionPositions.reserve(data.vertices.size());
		for (const auto& v : data.vertices) { mCollisionPositions.push_back(v.position); }
		mCollisionIndices = data.indices;

		CreateGpuBuffers(data.vertices, mHasSkinning ? data.skins : std::vector<SkinVertex>{}, data.indices);
		Logger::Info(LogCat::Asset, "Loaded model: {} ({} meshes, {} verts, {} clips)", path, mSubMeshes.size(), data.vertices.size(), mClips.size());
	}

	// 埋め込みのテクスチャを AssetManager に登録し、マテリアルのパスを登録したキーへ置き換える
	void Model::RegisterEmbeddedTextures(ModelData& data)
	{
		std::unordered_set<std::string> registered;
		auto resolve = [&](std::string& path)
			{
				if (!path.starts_with(ModelData::kEmbeddedPrefix)) { return; }
				bool srgb = !EndsWith(path, "#linear");
				size_t index = std::strtoull(path.c_str() + std::strlen(ModelData::kEmbeddedPrefix), nullptr, 10);
				const EmbeddedTexture& source = data.embeddedTextures[index];
				std::string key = std::format("{}#{}{}", mPath, source.sourcePath, srgb ? "" : "#linear");
				path = key;
				if (!registered.insert(key).second) { return; }

				auto tex = std::make_shared<Texture>();
				AssetManager::Get().Register(key, tex);
				if (source.height == 0) { tex->LoadFromMemory(source.data.data(), source.data.size(), srgb); }
				else { tex->LoadFromPixels(source.data.data(), source.width, source.height, srgb); }
			};
		for (auto& material : data.materials)
		{
			resolve(material.albedoPath);
			resolve(material.normalPath);
		}
	}

	bool Model::LoadBuiltin()
	{
		std::vector<MeshVertex> v;
		std::vector<uint32_t> i;
		if (mPath == "Engine/Cube") { Primitives::Cube(v, i); }
		else if (mPath == "Engine/Sphere") { Primitives::Sphere(v, i); }
		else if (mPath == "Engine/Plane") { Primitives::Plane(v, i); }
		else if (mPath == "Engine/Capsule") { Primitives::Capsule(v, i); }
		else
		{
			Logger::Error(LogCat::Asset, "Unknown built-in model: {}", mPath);
			return false;
		}
		BuildFromData(std::move(v), std::move(i));
		return true;
	}

	void Model::BuildFromData(std::vector<MeshVertex> vertices, std::vector<uint32_t> indices)
	{
		MeshUtil::FixWinding(vertices, indices);
		MeshUtil::ComputeTangents(vertices, indices);
		SubMesh sub;
		sub.name = GetFileName(mPath);
		sub.indexCount = uint32_t(indices.size());
		for (const auto& v : vertices) { sub.bounds.Expand(v.position); }
		mBounds = sub.bounds;
		mSubMeshes = { sub };
		mMaterials = { MaterialDesc{ "Default" } };
		mCollisionPositions.clear();
		for (const auto& v : vertices) { mCollisionPositions.push_back(v.position); }
		mCollisionIndices = indices;
		CreateGpuBuffers(vertices, {}, indices);
	}

	// すべてのバッファの転送が終わったら Ready にする（残りの数を共有のカウンタで数える）
	void Model::CreateGpuBuffers(const std::vector<MeshVertex>& vertices, const std::vector<SkinVertex>& skins,
		const std::vector<uint32_t>& indices)
	{
		auto& device = GraphicsDevice::Get();
		auto self = std::static_pointer_cast<Model>(shared_from_this());
		auto remaining = std::make_shared<std::atomic<int>>(skins.empty() ? 2 : 3);
		auto onComplete = [self, remaining]
			{
				if (remaining->fetch_sub(1) == 1) { self->SetState(AssetState::Ready); }
			};

		uint64_t vbSize = sizeof(MeshVertex) * vertices.size();
		mVertexBuffer = device.CreateStaticBuffer(vertices.data(), vbSize, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, onComplete);
		mVertexView = { mVertexBuffer->GetGPUVirtualAddress(), UINT(vbSize), sizeof(MeshVertex) };

		uint64_t ibSize = sizeof(uint32_t) * indices.size();
		mIndexBuffer = device.CreateStaticBuffer(indices.data(), ibSize, D3D12_RESOURCE_STATE_INDEX_BUFFER, onComplete);
		mIndexView = { mIndexBuffer->GetGPUVirtualAddress(), UINT(ibSize), DXGI_FORMAT_R32_UINT };

		if (!skins.empty())
		{
			uint64_t sbSize = sizeof(SkinVertex) * skins.size();
			mSkinBuffer = device.CreateStaticBuffer(skins.data(), sbSize, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, onComplete);
			mSkinView = { mSkinBuffer->GetGPUVirtualAddress(), UINT(sbSize), sizeof(SkinVertex) };
		}
	}
}
