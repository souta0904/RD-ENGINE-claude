#pragma once
#include "Asset/Asset.h"
#include "Core/Reflection/PropertyVisitor.h"
#include "Render/GpuData.h"
#include "RHI/Pipeline/PipelineBuilder.h"
#include <string>

namespace rd
{
	class Texture;

	// 陰影の付け方
	//  Standard: PBR（GGX）。Flat: 面ごとの平らな陰を、先頭の平行光源の向きで影の色から白へ塗り分け、縁を光らせる（トゥーン調）
	enum class ShadingModel { Standard, Flat };

	// 描画のマテリアル（albedo / normal / emissive はパスから読み込んだテクスチャ）
	//  発光マップがあるときは、発光の色にマップの色を掛ける
	//  triplanar にすると、アルベドを UV ではなくワールド座標で 3 方向から貼る（tile は 1 単位あたりの繰り返し数）
	struct Material
	{
		Color baseColor = Color::kWhite;
		ShadingModel shading = ShadingModel::Standard;
		Color shadowColor = Color(0.6f, 0.6f, 0.7f, 1.0f);
		Color rimColor = Color::kBlack;
		float rimPower = 3.0f;
		bool triplanar = false;
		float triplanarTile = 1.0f;
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
