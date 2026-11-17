#pragma once
#include "Math/Color.h"
#include "Math/Matrix4.h"
#include "Math/Vector4.h"

// シェーダーへ渡す定数（Assets/Engine/Shaders/Common.hlsli と並びを合わせる）
namespace rd
{
	inline constexpr uint32_t kMaxDirLights = 4;
	inline constexpr uint32_t kMaxPointLights = 32;
	inline constexpr uint32_t kMaxSpotLights = 16;
	// シャドウマップは 2x2 のタイルに分けて 1 枚にまとめる
	inline constexpr uint32_t kMaxShadowCascades = 4;
	// 1 つの物に効かせられる追加のライトの数の上限と、スポットライトの番号に付ける印
	inline constexpr uint32_t kMaxObjectLights = 8;
	inline constexpr uint32_t kObjectLightSpot = 0x8000u;

	enum MaterialFlags : uint32_t
	{
		kMatHasAlbedo = 1u << 0,
		kMatHasNormal = 1u << 1,
		kMatUnlit = 1u << 2,
		kMatAlphaClip = 1u << 3,
		kMatReceiveShadow = 1u << 4,
		kMatHasEmissive = 1u << 5,
		kMatFlat = 1u << 6,
		kMatTriplanar = 1u << 7,
	};

	struct GpuObjectData
	{
		Matrix4 world;
		Matrix4 worldInvTranspose;
		Color tint = Color::kWhite;
		// この物に効く追加のライト（Lights Per Object が 0 より大きいときだけ使う）
		//  番号の上の位（kObjectLightSpot）が立っていればスポットライト、なければ点光源
		uint32_t lights[kMaxObjectLights] = {};
		uint32_t lightCount = 0;
		uint32_t objectPad[3] = {};
	};
	static_assert(sizeof(GpuObjectData) == 192);

	// shadowViewProj: カスケードごと（タイルへの配置込み）、cascadeSplits: 各カスケードの奥の境界（ビュー空間の深さ）
	// cascadeTexelSizes: 1 テクセルのワールドでの大きさ、shadowCascade: シャドウパスで描いているカスケード
	// shadowFadeStart: ここから shadowDistance までで影を薄くする
	struct GpuFrameData
	{
		Matrix4 view;
		Matrix4 proj;
		Matrix4 viewProj;
		Matrix4 invViewProj;
		Matrix4 shadowViewProj[kMaxShadowCascades];
		Vector4 cascadeSplits;
		Vector4 cascadeTexelSizes;
		Vector3 cameraPos; float time = 0.0f;
		Vector3 cameraRight; float shadowBias = 0.0f;
		Vector3 cameraUp; float shadowStrength = 1.0f;
		Vector3 ambientSky; uint32_t cascadeCount = 0;
		Vector3 ambientGround; float fogDensity = 0.0f;
		Vector3 fogColor; float fogStart = 0.0f;
		Vector3 skyZenith; uint32_t dirLightCount = 0;
		Vector3 skyHorizon; uint32_t pointLightCount = 0;
		Vector2 screenSize; uint32_t spotLightCount = 0; uint32_t shadowEnabled = 0;
		uint32_t shadowCascade = 0;
		uint32_t debugCascades = 0;
		float shadowFadeStart = 0.0f;
		float pad = 0.0f;
		// 地平線の環境光（w が 1 なら空・地平線・地面の 3 色、0 なら空と地面の 2 色）
		Vector4 ambientEquator;
		// 追加のライト（点光源・スポットライト）を頂点ごとに計算するか（1 なら頂点）と、1 つの物に効く数（0 なら全部）
		uint32_t additionalLightsPerVertex = 0;
		uint32_t lightsPerObject = 0;
		uint32_t framePad2[2] = {};
	};
	static_assert(sizeof(GpuFrameData) == 736);

	struct GpuMaterialData
	{
		Color baseColor;
		Vector3 emissive; float metallic = 0.0f;
		float roughness = 0.5f; float alphaCutoff = 0.5f; uint32_t flags = 0; float triplanarTile = 1.0f;
		// Flat の陰影（影の色と、縁を光らせる色と鋭さ）
		Vector3 shadowColor; float rimPower = 3.0f;
		Vector3 rimColor; float pad = 0.0f;
	};
	static_assert(sizeof(GpuMaterialData) == 80);

	struct GpuDirLight
	{
		Vector3 direction; float intensity = 1.0f;
		Vector3 color; uint32_t castShadow = 0;
	};
	struct GpuPointLight
	{
		Vector3 position; float range = 10.0f;
		Vector3 color; float intensity = 1.0f;
	};
	struct GpuSpotLight
	{
		Vector3 position; float range = 10.0f;
		Vector3 direction; float intensity = 1.0f;
		Vector3 color; float cosInner = 0.9f;
		float cosOuter = 0.8f; Vector3 pad;
	};
	struct GpuLightData
	{
		GpuDirLight dirLights[kMaxDirLights];
		GpuPointLight pointLights[kMaxPointLights];
		GpuSpotLight spotLights[kMaxSpotLights];
	};
	static_assert(sizeof(GpuDirLight) == 32 && sizeof(GpuPointLight) == 32 && sizeof(GpuSpotLight) == 64);

	struct GpuParticle
	{
		Vector3 position; float size = 1.0f;
		Color color;
		float rotation = 0.0f; Vector3 pad;
	};
	static_assert(sizeof(GpuParticle) == 48);
}
