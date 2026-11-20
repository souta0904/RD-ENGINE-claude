#pragma once
#include "Asset/Asset.h"
#include "Core/Reflection/PropertyVisitor.h"
#include "Render/GpuData.h"
#include "RHI/Pipeline/PipelineBuilder.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace rd
{
	class Texture;
	class ShaderAsset;
	struct ShaderProperty;

	// 自作のシェーダーの値 1 つ分（数と色は value、テクスチャは texturePath）
	struct MaterialPropertyValue
	{
		Vector4 value;
		std::string texturePath;
		AssetRef<Texture> texture;
	};

	// 陰影の付け方
	//  Standard: PBR（GGX）。Flat: 面ごとの平らな陰を、先頭の平行光源の向きで影の色から白へ塗り分け、縁を光らせる（トゥーン調）
	enum class ShadingModel { Standard, Flat };

	// 描画のマテリアル（albedo / normal / emissive はパスから読み込んだテクスチャ）
	//  発光マップがあるときは、発光の色にマップの色を掛ける
	//  triplanar にすると、アルベドを UV ではなくワールド座標で 3 方向から貼る（tile は 1 単位あたりの繰り返し数）
	//
	//  shaderPath に自作のシェーダー（Surface.hlsli を使う .hlsl）を入れると、そのシェーダーで描く。
	//  シェーダーの Properties に書いた値は properties に名前で入り、画面では「Shader Properties」に並ぶ。
	//  標準の値（Base Color など）は自作のシェーダーでも使われ、Surface 関数に渡る値のはじめになる
	struct RD_API Material
	{
		std::string shaderPath;
		AssetRef<ShaderAsset> shader;
		std::unordered_map<std::string, MaterialPropertyValue> properties;

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
		// パスが変わっていればテクスチャ（と自作のシェーダー、その値のテクスチャ）を読み込み直す
		void ResolveTextures();
		GpuMaterialData ToGpu() const;

		// 描くときに使う自作のシェーダー（読めていなければ nullptr = 標準のシェーダーで描く）
		const ShaderAsset* GetCustomShader() const;
		// シェーダーの Properties の並びで、数と色を 16 バイトずつ詰める（b4 の定数バッファの中身）
		void BuildShaderConstants(std::vector<Vector4>& out) const;
		// t5 から数えて slot 番目のテクスチャ（なければ nullptr。代わりは既定値の名前で決める）
		const Texture* GetShaderTexture(int slot) const;

	private:
		void ResolveShader();
		// 値がなければシェーダーの既定値で作る
		MaterialPropertyValue& GetOrCreateProperty(const ShaderProperty& p);
		void ReflectShaderProperties(PropertyVisitor& v);
	};
}
