#include "RdPch.h"
#include "Asset/Model/ModelImporter.h"
#include "Core/Util/StringUtil.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace rd::ModelImporter
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

		// スケルトンはノードの階層をそのまま関節にする
		void ImportSkeleton(const aiNode* node, int32_t parent, Skeleton& skeleton)
		{
			Joint joint;
			joint.name = node->mName.C_Str();
			joint.parent = parent;
			ToMatrix(node->mTransformation).Decompose(joint.bindLocal.scale, joint.bindLocal.rotation, joint.bindLocal.translation);
			int32_t index = int32_t(skeleton.joints.size());
			skeleton.nameToIndex.emplace(joint.name, index);
			skeleton.joints.push_back(std::move(joint));
			for (unsigned c = 0; c < node->mNumChildren; ++c) { ImportSkeleton(node->mChildren[c], index, skeleton); }
		}

		void ImportAnimations(const aiScene* scene, ModelData& out)
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
				out.clips.push_back(std::move(clip));
			}
		}

		// 埋め込みのテクスチャを ModelData へ写す（同じものは 1 回だけ）。返すのは embeddedTextures の位置
		size_t AddEmbeddedTexture(const aiTexture* src, const std::string& texPath,
			std::unordered_map<const aiTexture*, size_t>& known, ModelData& out)
		{
			if (auto it = known.find(src); it != known.end()) { return it->second; }

			EmbeddedTexture tex;
			tex.name = GetStem(src->mFilename.length > 0 ? src->mFilename.C_Str() : texPath.c_str());
			tex.sourcePath = texPath;
			if (src->mHeight == 0)
			{
				// mHeight == 0 は png などの圧縮された画像（mWidth がバイト数）
				tex.format = ToLower(src->achFormatHint);
				auto bytes = reinterpret_cast<const uint8_t*>(src->pcData);
				tex.data.assign(bytes, bytes + src->mWidth);
			}
			else
			{
				tex.width = src->mWidth;
				tex.height = src->mHeight;
				size_t texels = size_t(src->mWidth) * src->mHeight;
				tex.data.resize(texels * 4);
				for (size_t t = 0; t < texels; ++t)
				{
					const aiTexel& texel = src->pcData[t];
					tex.data[t * 4 + 0] = texel.r;
					tex.data[t * 4 + 1] = texel.g;
					tex.data[t * 4 + 2] = texel.b;
					tex.data[t * 4 + 3] = texel.a;
				}
			}
			size_t index = out.embeddedTextures.size();
			out.embeddedTextures.push_back(std::move(tex));
			known.emplace(src, index);
			return index;
		}

		// 埋め込みなら "embedded:番号" を、外部のファイルならそのパスを返す（見つからなければファイル名だけで探す）
		std::string ImportTexture(const aiScene* scene, const std::string& dir, const std::string& texPath, bool srgb,
			std::unordered_map<const aiTexture*, size_t>& known, ModelData& out)
		{
			if (texPath.empty()) { return {}; }
			std::string result;
			if (const aiTexture* embedded = scene->GetEmbeddedTexture(texPath.c_str()))
			{
				result = ModelData::kEmbeddedPrefix + std::to_string(AddEmbeddedTexture(embedded, texPath, known, out));
			}
			else
			{
				result = NormalizePath(dir + texPath);
				if (!std::filesystem::exists(ToWide(result))) { result = dir + GetFileName(NormalizePath(texPath)); }
			}
			return srgb ? result : result + "#linear";
		}

		void ImportMaterials(const aiScene* scene, const std::string& path, ModelData& out)
		{
			std::string dir = GetDirectory(path);
			std::unordered_map<const aiTexture*, size_t> known;
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
					mat.albedoPath = ImportTexture(scene, dir, texPath.C_Str(), true, known, out);
				}
				if (src->GetTexture(aiTextureType_NORMALS, 0, &texPath) == AI_SUCCESS ||
					(EndsWith(ToLower(path), ".obj") && src->GetTexture(aiTextureType_HEIGHT, 0, &texPath) == AI_SUCCESS))
				{
					mat.normalPath = ImportTexture(scene, dir, texPath.C_Str(), false, known, out);
				}
				out.materials.push_back(std::move(mat));
			}
			if (out.materials.empty()) { out.materials.emplace_back(); }
		}

		// ノードを辿ってメッシュを 1 つの頂点バッファにまとめる。静的メッシュはノードの変換を焼き込む
		void ImportMeshes(const aiScene* scene, const aiNode* node, const Matrix4& parentGlobal, ModelData& out)
		{
			Matrix4 global = ToMatrix(node->mTransformation) * parentGlobal;
			for (unsigned n = 0; n < node->mNumMeshes; ++n)
			{
				const aiMesh* mesh = scene->mMeshes[node->mMeshes[n]];
				if (!(mesh->mPrimitiveTypes & aiPrimitiveType_TRIANGLE)) { continue; }
				bool skinned = mesh->HasBones() && out.skeleton;
				uint32_t base = uint32_t(out.vertices.size());

				SubMesh sub;
				sub.name = mesh->mName.C_Str();
				sub.materialIndex = std::min(mesh->mMaterialIndex, uint32_t(out.materials.size() - 1));
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
					out.hasSkinning = true;
					for (unsigned b = 0; b < mesh->mNumBones; ++b)
					{
						const aiBone* bone = mesh->mBones[b];
						int32_t joint = out.skeleton->Find(bone->mName.C_Str());
						if (joint < 0) { continue; }
						out.skeleton->joints[size_t(joint)].inverseBind = ToMatrix(bone->mOffsetMatrix);
						for (unsigned w = 0; w < bone->mNumWeights; ++w)
						{
							AddBoneWeight(out.skins[base + bone->mWeights[w].mVertexId], uint32_t(joint), bone->mWeights[w].mWeight);
						}
					}
					for (uint32_t v = base; v < uint32_t(out.skins.size()); ++v) { NormalizeWeights(out.skins[v]); }
				}
				out.bounds.Expand(sub.bounds);
				out.subMeshes.push_back(std::move(sub));
			}
			for (unsigned c = 0; c < node->mNumChildren; ++c) { ImportMeshes(scene, node->mChildren[c], global, out); }
		}
	}

	bool Import(const std::string& path, bool animOnly, ModelData& out, std::string& error)
	{
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
			error = importer.GetErrorString();
			return false;
		}

		bool hasBones = false;
		for (unsigned m = 0; m < scene->mNumMeshes; ++m) { hasBones |= scene->mMeshes[m]->HasBones(); }
		if (hasBones || scene->mNumAnimations > 0)
		{
			out.skeleton = std::make_shared<Skeleton>();
			ImportSkeleton(scene->mRootNode, -1, *out.skeleton);
		}
		ImportAnimations(scene, out);
		if (animOnly) { return true; }

		ImportMaterials(scene, path, out);
		ImportMeshes(scene, scene->mRootNode, Matrix4::kIdentity, out);
		return true;
	}
}
