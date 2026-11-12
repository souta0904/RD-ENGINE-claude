#include "RdPch.h"
#include "Asset/Model.h"
#include "Asset/AssetManager.h"
#include "Asset/Texture.h"
#include "Core/StringUtil.h"
#include "RHI/GraphicsDevice.h"
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

		// 時計回りを表面にそろえる（左手系 + D3D の既定カリング）
		void FixWinding(const std::vector<MeshVertex>& v, std::vector<uint32_t>& idx)
		{
			for (size_t i = 0; i + 2 < idx.size(); i += 3)
			{
				const auto& a = v[idx[i]];
				const auto& b = v[idx[i + 1]];
				const auto& c = v[idx[i + 2]];
				Vector3 n = Cross(b.position - a.position, c.position - a.position);
				if (Dot(n, a.normal + b.normal + c.normal) < 0.0f) { std::swap(idx[i + 1], idx[i + 2]); }
			}
		}

		// UV から接線を計算（プリミティブ用）
		void ComputeTangents(std::vector<MeshVertex>& v, const std::vector<uint32_t>& idx)
		{
			std::vector<Vector3> tan(v.size()), bitan(v.size());
			for (size_t i = 0; i + 2 < idx.size(); i += 3)
			{
				auto& a = v[idx[i]];
				auto& b = v[idx[i + 1]];
				auto& c = v[idx[i + 2]];
				Vector3 e1 = b.position - a.position, e2 = c.position - a.position;
				Vector2 d1 = b.uv - a.uv, d2 = c.uv - a.uv;
				float det = d1.x * d2.y - d2.x * d1.y;
				if (std::fabs(det) < 1.0e-8f) { continue; }
				float r = 1.0f / det;
				Vector3 t = (e1 * d2.y - e2 * d1.y) * r;
				Vector3 bt = (e2 * d1.x - e1 * d2.x) * r;
				for (int k = 0; k < 3; ++k)
				{
					tan[idx[i + k]] += t;
					bitan[idx[i + k]] += bt;
				}
			}
			for (size_t i = 0; i < v.size(); ++i)
			{
				Vector3 n = v[i].normal;
				Vector3 t = Normalize(ProjectOnPlane(tan[i], n));
				if (LengthSq(t) < 1.0e-6f) { t = Normalize(Cross(std::fabs(n.y) < 0.99f ? Vector3::kUnitY : Vector3::kUnitX, n)); }
				float w = Dot(Cross(n, t), bitan[i]) < 0.0f ? -1.0f : 1.0f;
				v[i].tangent = Vector4(t, w);
			}
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
		// 組み込みプリミティブ
		if (mPath.starts_with("Engine/"))
		{
			std::vector<MeshVertex> v;
			std::vector<uint32_t> i;
			if (mPath == "Engine/Cube") { Primitives::Cube(v, i); }
			else if (mPath == "Engine/Sphere") { Primitives::Sphere(v, i); }
			else if (mPath == "Engine/Plane") { Primitives::Plane(v, i); }
			else if (mPath == "Engine/Capsule") { Primitives::Capsule(v, i); }
			else
			{
				Log::Error("Unknown built-in model: {}", mPath);
				SetState(AssetState::Failed);
				return;
			}
			BuildFromData(std::move(v), std::move(i));
			return;
		}

		std::string path = mPath;
		bool animOnly = EndsWith(path, "#anim");
		if (animOnly) { path.resize(path.size() - std::strlen("#anim")); }
		std::string dir = GetDirectory(path);

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
			Log::Error("Failed to load model '{}': {}", path, importer.GetErrorString());
			SetState(AssetState::Failed);
			return;
		}

		// --------------------------------------------------
		// スケルトン（ノード階層をそのままジョイントにする）
		// --------------------------------------------------
		bool hasBones = false;
		for (unsigned m = 0; m < scene->mNumMeshes; ++m) { hasBones |= scene->mMeshes[m]->HasBones(); }
		if (hasBones || scene->mNumAnimations > 0)
		{
			mSkeleton = std::make_shared<Skeleton>();
			std::function<void(const aiNode*, int32_t)> visit = [&](const aiNode* node, int32_t parent)
				{
					Joint joint;
					joint.name = node->mName.C_Str();
					joint.parent = parent;
					ToMatrix(node->mTransformation).Decompose(joint.bindLocal.scale, joint.bindLocal.rotation, joint.bindLocal.translation);
					int32_t index = int32_t(mSkeleton->joints.size());
					mSkeleton->nameToIndex.emplace(joint.name, index);
					mSkeleton->joints.push_back(std::move(joint));
					for (unsigned c = 0; c < node->mNumChildren; ++c) { visit(node->mChildren[c], index); }
				};
			visit(scene->mRootNode, -1);
		}

		// --------------------------------------------------
		// アニメーション
		// --------------------------------------------------
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

		if (animOnly)
		{
			SetState(AssetState::Ready);
			return;
		}

		// --------------------------------------------------
		// マテリアル
		// --------------------------------------------------
		auto resolveTexture = [&](const aiString& texPath, bool srgb) -> std::string
			{
				std::string p = texPath.C_Str();
				if (p.empty()) { return {}; }
				if (const aiTexture* embedded = scene->GetEmbeddedTexture(texPath.C_Str()))
				{
					// 埋め込みテクスチャはここで読み込んで登録する
					std::string key = mPath + "#" + p + (srgb ? "" : "#linear");
					auto tex = std::make_shared<Texture>();
					AssetManager::Get().Register(key, tex);
					if (embedded->mHeight == 0)
					{
						tex->LoadFromMemory(embedded->pcData, embedded->mWidth, srgb);
					}
					else
					{
						std::vector<uint8_t> rgba(size_t(embedded->mWidth) * embedded->mHeight * 4);
						for (size_t t = 0; t < size_t(embedded->mWidth) * embedded->mHeight; ++t)
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
				std::string full = NormalizePath(dir + p);
				if (!std::filesystem::exists(ToWide(full))) { full = dir + GetFileName(p); }
				return srgb ? full : full + "#linear";
			};

		for (unsigned i = 0; i < scene->mNumMaterials; ++i)
		{
			const aiMaterial* src = scene->mMaterials[i];
			MaterialDesc mat;
			aiString name;
			if (src->Get(AI_MATKEY_NAME, name) == AI_SUCCESS) { mat.name = name.C_Str(); }

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
				// テクスチャなしの metallic=1 は glTF の既定値であることが多いので弱める
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

			aiString texPath;
			if (src->GetTexture(aiTextureType_BASE_COLOR, 0, &texPath) == AI_SUCCESS ||
				src->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS)
			{
				mat.albedoPath = resolveTexture(texPath, true);
			}
			if (src->GetTexture(aiTextureType_NORMALS, 0, &texPath) == AI_SUCCESS ||
				(EndsWith(ToLower(path), ".obj") && src->GetTexture(aiTextureType_HEIGHT, 0, &texPath) == AI_SUCCESS))
			{
				mat.normalPath = resolveTexture(texPath, false);
			}
			mMaterials.push_back(std::move(mat));
		}
		if (mMaterials.empty()) { mMaterials.emplace_back(); }

		// --------------------------------------------------
		// メッシュ
		// --------------------------------------------------
		std::vector<MeshVertex> vertices;
		std::vector<SkinVertex> skins;
		std::vector<uint32_t> indices;

		std::function<void(const aiNode*, const Matrix4&)> visitMesh = [&](const aiNode* node, const Matrix4& parentGlobal)
			{
				Matrix4 global = ToMatrix(node->mTransformation) * parentGlobal;
				for (unsigned n = 0; n < node->mNumMeshes; ++n)
				{
					const aiMesh* mesh = scene->mMeshes[node->mMeshes[n]];
					if (!(mesh->mPrimitiveTypes & aiPrimitiveType_TRIANGLE)) { continue; }
					bool skinned = mesh->HasBones() && mSkeleton;
					uint32_t base = uint32_t(vertices.size());

					SubMesh sub;
					sub.name = mesh->mName.C_Str();
					sub.materialIndex = std::min(mesh->mMaterialIndex, uint32_t(mMaterials.size() - 1));
					sub.skinned = skinned;
					sub.indexStart = uint32_t(indices.size());

					for (unsigned v = 0; v < mesh->mNumVertices; ++v)
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

						// 静的メッシュはノードの変換を焼き込む
						if (!skinned)
						{
							mv.position = TransformPoint(mv.position, global);
							mv.normal = Normalize(TransformVector(mv.normal, global));
							Vector3 t = Normalize(TransformVector(mv.tangent.XYZ(), global));
							mv.tangent = Vector4(t, mv.tangent.w);
						}
						sub.bounds.Expand(mv.position);
						vertices.push_back(mv);
						skins.emplace_back();
					}

					for (unsigned f = 0; f < mesh->mNumFaces; ++f)
					{
						const aiFace& face = mesh->mFaces[f];
						if (face.mNumIndices != 3) { continue; }
						indices.push_back(base + face.mIndices[0]);
						indices.push_back(base + face.mIndices[1]);
						indices.push_back(base + face.mIndices[2]);
					}
					sub.indexCount = uint32_t(indices.size()) - sub.indexStart;

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
								SkinVertex& sv = skins[base + bone->mWeights[w].mVertexId];
								// 最も小さいウェイトの枠を置き換える
								int slot = 0;
								for (int k = 1; k < 4; ++k) { if (sv.weights[k] < sv.weights[slot]) { slot = k; } }
								if (bone->mWeights[w].mWeight > sv.weights[slot])
								{
									sv.weights[slot] = bone->mWeights[w].mWeight;
									sv.joints[slot] = uint32_t(joint);
								}
							}
						}
						for (uint32_t v = base; v < uint32_t(skins.size()); ++v)
						{
							auto& sv = skins[v];
							float sum = sv.weights[0] + sv.weights[1] + sv.weights[2] + sv.weights[3];
							if (sum > 0.0f) { for (float& w : sv.weights) { w /= sum; } }
							else { sv.weights[0] = 1.0f; }
						}
					}
					mBounds.Expand(sub.bounds);
					mSubMeshes.push_back(std::move(sub));
				}
				for (unsigned c = 0; c < node->mNumChildren; ++c) { visitMesh(node->mChildren[c], global); }
			};
		visitMesh(scene->mRootNode, Matrix4::kIdentity);

		if (vertices.empty())
		{
			SetState(AssetState::Ready);
			return;
		}

		// 衝突判定用データ
		mCollisionPositions.reserve(vertices.size());
		for (const auto& v : vertices) { mCollisionPositions.push_back(v.position); }
		mCollisionIndices = indices;

		CreateGpuBuffers(vertices, mHasSkinning ? skins : std::vector<SkinVertex>{}, indices);
		Log::Info("Loaded model: {} ({} meshes, {} verts, {} clips)", path, mSubMeshes.size(), vertices.size(), mClips.size());
	}

	void Model::BuildFromData(std::vector<MeshVertex> vertices, std::vector<uint32_t> indices)
	{
		FixWinding(vertices, indices);
		ComputeTangents(vertices, indices);
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

	void Model::CreateGpuBuffers(const std::vector<MeshVertex>& vertices, const std::vector<SkinVertex>& skins,
		const std::vector<uint32_t>& indices)
	{
		auto& device = GraphicsDevice::Get();
		auto self = std::static_pointer_cast<Model>(shared_from_this());

		// すべてのバッファの転送が終わったら Ready
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

	// ==================================================
	// 組み込みプリミティブ
	// ==================================================
	namespace Primitives
	{
		void Cube(std::vector<MeshVertex>& v, std::vector<uint32_t>& idx)
		{
			const Vector3 normals[6] = { Vector3::kUnitX, -Vector3::kUnitX, Vector3::kUnitY, -Vector3::kUnitY, Vector3::kUnitZ, -Vector3::kUnitZ };
			for (const auto& n : normals)
			{
				Vector3 up = std::fabs(n.y) > 0.5f ? Vector3::kUnitZ : Vector3::kUnitY;
				Vector3 right = Cross(up, n);
				uint32_t base = uint32_t(v.size());
				Vector3 c = n * 0.5f;
				v.push_back({ c - right * 0.5f - up * 0.5f, n, {}, { 0.0f, 1.0f } });
				v.push_back({ c - right * 0.5f + up * 0.5f, n, {}, { 0.0f, 0.0f } });
				v.push_back({ c + right * 0.5f + up * 0.5f, n, {}, { 1.0f, 0.0f } });
				v.push_back({ c + right * 0.5f - up * 0.5f, n, {}, { 1.0f, 1.0f } });
				idx.insert(idx.end(), { base, base + 1, base + 2, base, base + 2, base + 3 });
			}
		}

		void Sphere(std::vector<MeshVertex>& v, std::vector<uint32_t>& idx, uint32_t segments, uint32_t rings)
		{
			for (uint32_t r = 0; r <= rings; ++r)
			{
				float phi = math::kPi * float(r) / float(rings);
				for (uint32_t s = 0; s <= segments; ++s)
				{
					float theta = math::k2Pi * float(s) / float(segments);
					Vector3 n(std::sin(phi) * std::cos(theta), std::cos(phi), std::sin(phi) * std::sin(theta));
					v.push_back({ n * 0.5f, n, {}, { float(s) / float(segments), float(r) / float(rings) } });
				}
			}
			for (uint32_t r = 0; r < rings; ++r)
			{
				for (uint32_t s = 0; s < segments; ++s)
				{
					uint32_t a = r * (segments + 1) + s;
					uint32_t b = a + segments + 1;
					idx.insert(idx.end(), { a, b, a + 1, a + 1, b, b + 1 });
				}
			}
		}

		void Plane(std::vector<MeshVertex>& v, std::vector<uint32_t>& idx, float size)
		{
			float h = size * 0.5f;
			float uv = size;// 1m ごとにタイル
			v.push_back({ { -h, 0.0f, -h }, Vector3::kUnitY, {}, { 0.0f, uv } });
			v.push_back({ { -h, 0.0f, h }, Vector3::kUnitY, {}, { 0.0f, 0.0f } });
			v.push_back({ { h, 0.0f, h }, Vector3::kUnitY, {}, { uv, 0.0f } });
			v.push_back({ { h, 0.0f, -h }, Vector3::kUnitY, {}, { uv, uv } });
			idx.insert(idx.end(), { 0, 1, 2, 0, 2, 3 });
		}

		void Capsule(std::vector<MeshVertex>& v, std::vector<uint32_t>& idx, float radius, float height)
		{
			const uint32_t segments = 32, halfRings = 8;
			float half = std::max(0.0f, height * 0.5f - radius);
			uint32_t rings = halfRings * 2 + 1;
			for (uint32_t r = 0; r <= rings; ++r)
			{
				// 上半球 -> 円柱 -> 下半球
				float phi;
				float offset;
				if (r <= halfRings) { phi = math::kPiOver2 * float(r) / float(halfRings); offset = half; }
				else { phi = math::kPiOver2 + math::kPiOver2 * float(r - halfRings - 1) / float(halfRings); offset = -half; }
				for (uint32_t s = 0; s <= segments; ++s)
				{
					float theta = math::k2Pi * float(s) / float(segments);
					Vector3 n(std::sin(phi) * std::cos(theta), std::cos(phi), std::sin(phi) * std::sin(theta));
					v.push_back({ n * radius + Vector3(0.0f, offset, 0.0f), n, {}, { float(s) / float(segments), float(r) / float(rings) } });
				}
			}
			for (uint32_t r = 0; r < rings; ++r)
			{
				for (uint32_t s = 0; s < segments; ++s)
				{
					uint32_t a = r * (segments + 1) + s;
					uint32_t b = a + segments + 1;
					idx.insert(idx.end(), { a, b, a + 1, a + 1, b, b + 1 });
				}
			}
		}
	}
}
