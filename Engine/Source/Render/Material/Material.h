#pragma once
#include "Asset/Asset.h"
#include "Core/Reflection/PropertyVisitor.h"
#include "Render/GpuData.h"
#include "RHI/Pipeline/PipelineBuilder.h"
#include <string>

namespace rd
{
	class Texture;

	// 描画のマテリアル（albedo / normal / emissive はパスから読み込んだテクスチャ）
	//  発光マップがあるときは、発光の色にマップの色を掛ける
	struct Material
	{
		Color baseColor = Color::kWhite;
		float metallic = 0.0f;
		float roughness = 0.6f;
		Color emissive = Color::kBlack;
		float emissiveIntensity = 1.0f;
		BlendMode blend = BlendMode::Opaque;
		bool doubleSided = false;
		bool unlit = false;
		bool alphaClip = false;
		float alphaCutoff = 0.5f;
		bool castShadows = true;
		bool receiveShadows = true;
		std::string albedoPath;
		std::string normalPath;
		std::string emissivePath;

		AssetRef<Texture> albedo;
		AssetRef<Texture> normal;
		AssetRef<Texture> emissiveMap;

		void Reflect(PropertyVisitor& v);
		// パスが変わっていればテクスチャを読み込み直す
		void ResolveTextures();
		GpuMaterialData ToGpu() const;
	};
}
