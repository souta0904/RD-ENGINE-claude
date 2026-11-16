#include "RdPch.h"
#include "Asset/Model/Model.h"
#include "Asset/AssetManager.h"
#include "Asset/Model/MeshUtil.h"
#include "Asset/Model/Primitives.h"
#include "Asset/Texture/Texture.h"
#include "Core/Util/StringUtil.h"
#include "RHI/Device/GraphicsDevice.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace rd
{
	namespace
	{
		// assimp は列ベクトル規約なので転置して取り込む
		Matrix4 ToMatrix(const aiMatrix4x4& a)
		{
			return {
				a.a1, a.b1, a.c1, a.d1,
				a.a2, a.b2, a.c2, a.d2,
				a.a3, a.b3, a.c3, a.d3,
				a.a4, a.b4, a.c4, a.d4 };
		}
		Vector3 ToVec3(const aiVector3D& v) { return { v.x, v.y, v.z }; }
		Quaternion ToQuat(const aiQuaternion& q) { return { q.w, q.x, q.y, q.z }; }
		Color ToColor(const aiColor4D& c) { return { c.r, c.g, c.b, c.a }; }

		MeshVertex ToVertex(const aiMesh* mesh, unsigned v)
		{
			MeshVertex mv;
			mv.position = ToVec3(mesh->mVertices[v]);
			mv.normal = mesh->HasNormals() ? ToVec3(mesh->mNormals[v]) : Vector3::kUnitY;
			if (mesh->HasTangentsAndBitangents())
			{
				Vector3 t = ToVec3(mesh->mTangents[v]);
				Vector3 b = ToVec3(mesh->mBitangents[v]);
				mv.tangent = Vector4(t, Dot(Cross(mv.normal, t), b) < 0.0f ? -1.0f : 1.0f);
			}
			else
			{
				mv.tangent = Vector4(1.0f, 0.0f, 0.0f, 1.0f);
			}
			if (mesh->HasTextureCoords(0)) { mv.uv = Vector2(mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y); }
			return mv;
		}

		// 1 頂点に 4 本まで。枠が埋まっていたら、一番小さいウェイトより大きいときだけ置き換える
		void AddBoneWeight(SkinVertex& sv, uint32_t joint, float weight)
		{
			int slot = 0;
			for (int k = 1; k < 4; ++k) { if (sv.weights[k] < sv.weights[slot]) { slot = k; } }
			if (weight > sv.weights[slot])
			{
				sv.weights[slot] = weight;
				sv.joints[slot] = joint;
			}
		}

		void NormalizeWeights(SkinVertex& sv)
		{
			float sum = sv.weights[0] + sv.weights[1] + sv.weights[2] + sv.weights[3];
			if (sum > 0.0f) { for (float& w : sv.weights) { w /= sum; } }
			else { sv.weights[0] = 1.0f; }
		}

		// glTF のテクスチャなしの metallic=1 は既定値であることが多いので弱める。shininess しかなければ粗さに直す
		void ImportSurface(const aiMaterial* src, MaterialDesc& mat)
		{
			aiColor4D color;
			if (src->Get(AI_MATKEY_BASE_COLOR, color) == AI_SUCCESS || src->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS)
			{
				mat.baseColor = ToColor(color);
			}
			float opacity = 1.0f;
			if (src->Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS && opacity < 0.999f)
			{
				mat.baseColor.a *= opacity;
				mat.transparent = true;
			}
			float metallic = 0.0f, roughness = 0.0f, shininess = 0.0f;
			bool hasMetallicTex = src->GetTextureCount(aiTextureType_METALNESS) > 0;
			if (src->Get(AI_MATKEY_METALLIC_FACTOR, metallic) == AI_SUCCESS)
			{
				mat.metallic = hasMetallicTex ? metallic : std::min(metallic, 0.2f);
			}
			if (src->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == AI_SUCCESS)
			{
				mat.roughness = std::clamp(roughness, 0.05f, 1.0f);
			}
			else if (src->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS && shininess > 0.0f)
			{
				mat.roughness = std::clamp(std::sqrt(2.0f / (shininess + 2.0f)), 0.05f, 1.0f);
			}
			if (src->Get(AI_MATKEY_COLOR_EMISSIVE, color) == AI_SUCCESS) { mat.emissive = ToColor(color); }
		}
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

		Assimp::Importer importer;
		unsigned flags = aiProcess_ConvertToLeftHanded;
		if (!animOnly)
		{
			flags |= aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace |
				aiProcess_JoinIdenticalVertices | aiProcess_LimitBoneWeights | aiProcess_SortByPType |
				aiProcess_ImproveCacheLocality | aiProcess_GenUVCoords;
		}
		const aiScene* scene = importer.ReadFile(path, flags);
		if (!scene || !scene->mRootNode)
		{
			Log::Error(LogCat::Asset, "Failed to load model '{}': {}", path, importer.GetErrorString());
			SetState(AssetState::Failed);
			return;
		}

		// スケルトンはノードの階層をそのまま関節にする
		bool hasBones = false;
		for (unsigned m = 0; m < scene->mNumMeshes; ++m) { hasBones |= scene->mMeshes[m]->HasBones(); }
		if (hasBones || scene->mNumAnimations > 0)
		{
			mSkeleton = std::make_shared<Skeleton>();
			ImportSkeleton(scene->mRootNode, -1);
		}
		ImportAnimations(scene);
		if (animOnly)
		{
			SetState(AssetState::Ready);
			return;
		}

		ImportMaterials(scene, path);
		MeshData mesh;
		ImportMeshes(scene, scene->mRootNode, Matrix4::kIdentity, mesh);
		if (mesh.vertices.empty())
		{
			SetState(AssetState::Ready);
			return;
		}

		mCollisionPositions.reserve(mesh.vertices.size());
		for (const auto& v : mesh.vertices) { mCollisionPositions.push_back(v.position); }
		mCollisionIndices = mesh.indices;

		CreateGpuBuffers(mesh.vertices, mHasSkinning ? mesh.skins : std::vector<SkinVertex>{}, mesh.indices);
		Log::Info(LogCat::Asset, "Loaded model: {} ({} meshes, {} verts, {} clips)", path, mSubMeshes.size(), mesh.vertices.size(), mClips.size());
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
			Log::Error(LogCat::Asset, "Unknown built-in model: {}", mPath);
			return false;
		}
		BuildFromData(std::move(v), std::move(i));
		return true;
	}

	void Model::ImportSkeleton(const aiNode* node, int32_t parent)
	{
		Joint joint;
		joint.name = node->mName.C_Str();
		joint.parent = parent;
		ToMatrix(node->mTransformation).Decompose(joint.bindLocal.scale, joint.bindLocal.rotation, joint.bindLocal.translation);
		int32_t index = int32_t(mSkeleton->joints.size());
		mSkeleton->nameToIndex.emplace(joint.name, index);
		mSkeleton->joints.push_back(std::move(joint));
		for (unsigned c = 0; c < node->mNumChildren; ++c) { ImportSkeleton(node->mChildren[c], index); }
	}

	void Model::ImportAnimations(const aiScene* scene)
	{
		for (unsigned a = 0; a < scene->mNumAnimations; ++a)
		{
			const aiAnimation* anim = scene->mAnimations[a];
			double tps = anim->mTicksPerSecond != 0.0 ? anim->mTicksPerSecond : 25.0;
			auto clip = std::make_shared<AnimationClip>();
			clip->name = anim->mName.length > 0 ? anim->mName.C_Str() : std::format("Clip{}", a);
			clip->duration = float(anim->mDuration / tps);
			for (unsigned c = 0; c < anim->mNumChannels; ++c)
			{
				const aiNodeAnim* src = anim->mChannels[c];
				AnimationChannel ch;
				ch.jointName = src->mNodeName.C_Str();
				for (unsigned k = 0; k < src->mNumPositionKeys; ++k)
				{
					ch.positions.push_back({ float(src->mPositionKeys[k].mTime / tps), ToVec3(src->mPositionKeys[k].mValue) });
				}
				for (unsigned k = 0; k < src->mNumRotationKeys; ++k)
				{
					ch.rotations.push_back({ float(src->mRotationKeys[k].mTime / tps), ToQuat(src->mRotationKeys[k].mValue) });
				}
				for (unsigned k = 0; k < src->mNumScalingKeys; ++k)
				{
					ch.scales.push_back({ float(src->mScalingKeys[k].mTime / tps), ToVec3(src->mScalingKeys[k].mValue) });
				}
				clip->channels.push_back(std::move(ch));
			}
			mClips.push_back(std::move(clip));
		}
	}

	void Model::ImportMaterials(const aiScene* scene, const std::string& path)
	{
		std::string dir = GetDirectory(path);
		for (unsigned i = 0; i < scene->mNumMaterials; ++i)
		{
			const aiMaterial* src = scene->mMaterials[i];
			MaterialDesc mat;
			aiString name;
			if (src->Get(AI_MATKEY_NAME, name) == AI_SUCCESS) { mat.name = name.C_Str(); }
			ImportSurface(src, mat);

			// obj は法線マップが HEIGHT として入っていることがある
			aiString texPath;
			if (src->GetTexture(aiTextureType_BASE_COLOR, 0, &texPath) == AI_SUCCESS ||
				src->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS)
			{
				mat.albedoPath = ImportTexture(scene, dir, texPath.C_Str(), true);
			}
			if (src->GetTexture(aiTextureType_NORMALS, 0, &texPath) == AI_SUCCESS ||
				(EndsWith(ToLower(path), ".obj") && src->GetTexture(aiTextureType_HEIGHT, 0, &texPath) == AI_SUCCESS))
			{
				mat.normalPath = ImportTexture(scene, dir, texPath.C_Str(), false);
			}
			mMaterials.push_back(std::move(mat));
		}
		if (mMaterials.empty()) { mMaterials.emplace_back(); }
	}

	// 埋め込みのテクスチャはここで読み込んで登録し、外部のファイルはパスを返す（見つからなければファイル名だけで探す）
	std::string Model::ImportTexture(const aiScene* scene, const std::string& dir, const std::string& texPath, bool srgb)
	{
		if (texPath.empty()) { return {}; }
		if (const aiTexture* embedded = scene->GetEmbeddedTexture(texPath.c_str()))
		{
			std::string key = mPath + "#" + texPath + (srgb ? "" : "#linear");
			auto tex = std::make_shared<Texture>();
			AssetManager::Get().Register(key, tex);
			if (embedded->mHeight == 0)
			{
				// mHeight == 0 は png などの圧縮された画像（mWidth がバイト数）
				tex->LoadFromMemory(embedded->pcData, embedded->mWidth, srgb);
			}
			else
			{
				size_t texels = size_t(embedded->mWidth) * embedded->mHeight;
				std::vector<uint8_t> rgba(texels * 4);
				for (size_t t = 0; t < texels; ++t)
				{
					const aiTexel& texel = embedded->pcData[t];
					rgba[t * 4 + 0] = texel.r;
					rgba[t * 4 + 1] = texel.g;
					rgba[t * 4 + 2] = texel.b;
					rgba[t * 4 + 3] = texel.a;
				}
				tex->LoadFromPixels(rgba.data(), embedded->mWidth, embedded->mHeight, srgb);
			}
			return key;
		}
		std::string full = NormalizePath(dir + texPath);
		if (!std::filesystem::exists(ToWide(full))) { full = dir + GetFileName(texPath); }
		return srgb ? full : full + "#linear";
	}

	// ノードを辿ってメッシュを 1 つの頂点バッファにまとめる。静的メッシュはノードの変換を焼き込む
	void Model::ImportMeshes(const aiScene* scene, const aiNode* node, const Matrix4& parentGlobal, MeshData& out)
	{
		Matrix4 global = ToMatrix(node->mTransformation) * parentGlobal;
		for (unsigned n = 0; n < node->mNumMeshes; ++n)
		{
			const aiMesh* mesh = scene->mMeshes[node->mMeshes[n]];
			if (!(mesh->mPrimitiveTypes & aiPrimitiveType_TRIANGLE)) { continue; }
			bool skinned = mesh->HasBones() && mSkeleton;
			uint32_t base = uint32_t(out.vertices.size());

			SubMesh sub;
			sub.name = mesh->mName.C_Str();
			sub.materialIndex = std::min(mesh->mMaterialIndex, uint32_t(mMaterials.size() - 1));
			sub.skinned = skinned;
			sub.indexStart = uint32_t(out.indices.size());

			for (unsigned v = 0; v < mesh->mNumVertices; ++v)
			{
				MeshVertex mv = ToVertex(mesh, v);
				if (!skinned)
				{
					mv.position = TransformPoint(mv.position, global);
					mv.normal = Normalize(TransformVector(mv.normal, global));
					Vector3 t = Normalize(TransformVector(mv.tangent.XYZ(), global));
					mv.tangent = Vector4(t, mv.tangent.w);
				}
				sub.bounds.Expand(mv.position);
				out.vertices.push_back(mv);
				out.skins.emplace_back();
			}

			for (unsigned f = 0; f < mesh->mNumFaces; ++f)
			{
				const aiFace& face = mesh->mFaces[f];
				if (face.mNumIndices != 3) { continue; }
				out.indices.push_back(base + face.mIndices[0]);
				out.indices.push_back(base + face.mIndices[1]);
				out.indices.push_back(base + face.mIndices[2]);
			}
			sub.indexCount = uint32_t(out.indices.size()) - sub.indexStart;

			if (skinned)
			{
				mHasSkinning = true;
				for (unsigned b = 0; b < mesh->mNumBones; ++b)
				{
					const aiBone* bone = mesh->mBones[b];
					int32_t joint = mSkeleton->Find(bone->mName.C_Str());
					if (joint < 0) { continue; }
					mSkeleton->joints[size_t(joint)].inverseBind = ToMatrix(bone->mOffsetMatrix);
					for (unsigned w = 0; w < bone->mNumWeights; ++w)
					{
						AddBoneWeight(out.skins[base + bone->mWeights[w].mVertexId], uint32_t(joint), bone->mWeights[w].mWeight);
					}
				}
				for (uint32_t v = base; v < uint32_t(out.skins.size()); ++v) { NormalizeWeights(out.skins[v]); }
			}
			mBounds.Expand(sub.bounds);
			mSubMeshes.push_back(std::move(sub));
		}
		for (unsigned c = 0; c < node->mNumChildren; ++c) { ImportMeshes(scene, node->mChildren[c], global, out); }
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
