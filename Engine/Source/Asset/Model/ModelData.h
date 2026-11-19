#pragma once
#include "Asset/Animation/AnimationClip.h"
#include "Asset/Model/MaterialDesc.h"
#include "Asset/Model/MeshVertex.h"
#include "Asset/Model/SubMesh.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace rd
{
	// モデルファイルに埋め込まれていたテクスチャ
	//  height が 0 なら data は png などの圧縮された画像そのもの、0 より大きければ RGBA8 の画素の並び
	struct EmbeddedTexture
	{
		std::string name;
		std::string sourcePath; // モデルの中での呼び名（"*0" など）
		std::string format; // 圧縮された画像の種類（"png" など。分からなければ空）
		uint32_t width = 0;
		uint32_t height = 0;
		std::vector<uint8_t> data;
	};

	// GPU に送る前のモデルの中身（メインスレッドや GPU に触らないので、ワーカースレッドで作れる）
	//  fbx などからの読み込みと .rdmodel の読み書きは、すべてこの形を通す
	struct ModelData
	{
		// マテリアルのテクスチャのパスがこの文字で始まるときは、後ろの番号が embeddedTextures の位置を表す
		static constexpr const char* kEmbeddedPrefix = "embedded:";

		std::vector<SubMesh> subMeshes;
		std::vector<MaterialDesc> materials;
		std::shared_ptr<Skeleton> skeleton;
		std::vector<std::shared_ptr<AnimationClip>> clips;
		std::vector<EmbeddedTexture> embeddedTextures;
		AABB bounds;
		bool hasSkinning = false;

		std::vector<MeshVertex> vertices;
		std::vector<SkinVertex> skins;
		std::vector<uint32_t> indices;
	};
}
