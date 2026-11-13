#pragma once
#include "Asset/Asset.h"
#include "Core/Reflection.h"
#include "Math/Geometry.h"
#include "RHI/Pipeline.h"
#include <vector>

namespace rd
{
	class Model;
	class Texture;

	// ==================================================
	// GPU 定数（Assets/Engine/Shaders/Common.hlsli と一致させる）
	// ==================================================
	inline constexpr uint32_t kMaxDirLights = 4;
	inline constexpr uint32_t kMaxPointLights = 32;
	inline constexpr uint32_t kMaxSpotLights = 16;

	enum MaterialFlags : uint32_t
	{
		kMatHasAlbedo = 1u << 0,
		kMatHasNormal = 1u << 1,
		kMatUnlit = 1u << 2,
		kMatAlphaClip = 1u << 3,
		kMatReceiveShadow = 1u << 4,
	};

	struct GpuObjectData
	{
		Matrix4 world;
		Matrix4 worldInvTranspose;
		Color tint = Color::kWhite;
	};
	static_assert(sizeof(GpuObjectData) == 144);

	// カスケードシャドウの最大数（シャドウマップは 2x2 のタイルに分けて 1 枚にまとめる）
	inline constexpr uint32_t kMaxShadowCascades = 4;

	struct GpuFrameData
	{
		Matrix4 view;
		Matrix4 proj;
		Matrix4 viewProj;
		Matrix4 invViewProj;
		Matrix4 shadowViewProj[kMaxShadowCascades];// カスケードごと（タイルへの配置込み）
		Vector4 cascadeSplits;// 各カスケードの奥の境界（ビュー空間の深さ）
		Vector4 cascadeTexelSizes;// 各カスケードの 1 テクセルのワールドでの大きさ
		Vector3 cameraPos; float time = 0.0f;
		Vector3 cameraRight; float shadowBias = 0.0f;
		Vector3 cameraUp; float shadowStrength = 1.0f;
		Vector3 ambientSky; uint32_t cascadeCount = 0;
		Vector3 ambientGround; float fogDensity = 0.0f;
		Vector3 fogColor; float fogStart = 0.0f;
		Vector3 skyZenith; uint32_t dirLightCount = 0;
		Vector3 skyHorizon; uint32_t pointLightCount = 0;
		Vector2 screenSize; uint32_t spotLightCount = 0; uint32_t shadowEnabled = 0;
		uint32_t shadowCascade = 0;// シャドウパスで描いているカスケード
		uint32_t debugCascades = 0;// カスケードを色分けして表示
		float shadowFadeStart = 0.0f;// ここから shadowDistance までで影を薄くする
		float pad = 0.0f;
	};
	static_assert(sizeof(GpuFrameData) == 704);

	struct GpuMaterialData
	{
		Color baseColor;
		Vector3 emissive; float metallic = 0.0f;
		float roughness = 0.5f; float alphaCutoff = 0.5f; uint32_t flags = 0; float pad = 0.0f;
	};
	static_assert(sizeof(GpuMaterialData) == 48);

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

	// ==================================================
	// 描画用マテリアル
	// ==================================================
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

		// 実行時に解決されるテクスチャ
		AssetRef<Texture> albedo;
		AssetRef<Texture> normal;

		void Reflect(PropertyVisitor& v);
		// パスが変わっていればテクスチャを読み込み直す
		void ResolveTextures();
		GpuMaterialData ToGpu() const;
	};

	// ==================================================
	// 描画リスト（コンポーネントが毎フレーム積み、Renderer が描く）
	// ==================================================
	struct MeshDrawItem
	{
		const Model* model = nullptr;
		uint32_t subMesh = 0;
		Matrix4 world;
		const Material* material = nullptr;
		D3D12_GPU_VIRTUAL_ADDRESS palette = 0;// スキニング行列（0 なら静的）
		AABB worldBounds;
		Color tint = Color::kWhite;
		float sortKey = 0.0f;
	};

	struct ParticleDrawItem
	{
		D3D12_GPU_VIRTUAL_ADDRESS instances = 0;
		uint32_t count = 0;
		const Texture* texture = nullptr;
		BlendMode blend = BlendMode::Additive;
		Color color = Color::kWhite;
		Color emissive = Color::kBlack;
		Vector3 center;
	};

	struct SpriteDrawItem
	{
		const Texture* texture = nullptr;
		Vector2 position;// スクリーン座標（左上原点、ピクセル）
		Vector2 size = { 100.0f, 100.0f };
		Vector2 pivot = { 0.5f, 0.5f };
		float rotation = 0.0f;
		Color color = Color::kWhite;
		Vector4 uvRect = { 0.0f, 0.0f, 1.0f, 1.0f };// u0, v0, u1, v1
		int32_t layer = 0;
	};

	class RenderList
	{
	public:
		void Clear()
		{
			meshes.clear();
			particles.clear();
			sprites.clear();
			dirLights.clear();
			pointLights.clear();
			spotLights.clear();
		}

		std::vector<MeshDrawItem> meshes;
		std::vector<ParticleDrawItem> particles;
		std::vector<SpriteDrawItem> sprites;
		std::vector<GpuDirLight> dirLights;
		std::vector<GpuPointLight> pointLights;
		std::vector<GpuSpotLight> spotLights;
	};

	// ==================================================
	// カメラと環境
	// ==================================================
	struct RenderCamera
	{
		Matrix4 view;
		Matrix4 proj;
		Vector3 position;
		Quaternion rotation;
		float fovY = math::ToRadians(60.0f);
		float nearZ = 0.1f;
		float farZ = 1000.0f;

		// ビュー行列と射影行列を作る
		static RenderCamera Make(const Vector3& position, const Quaternion& rotation, float fovY, float aspect, float nearZ, float farZ);
		// スクリーン座標（0～1）からワールドのレイを作る
		Ray ScreenPointToRay(const Vector2& uv) const;
		Vector3 GetForward() const { return Vector3::kUnitZ * rotation; }
		Vector3 GetRight() const { return Vector3::kUnitX * rotation; }
		Vector3 GetUp() const { return Vector3::kUnitY * rotation; }
	};

	// シーンごとの環境設定
	struct Environment
	{
		Color ambientSky = { 0.35f, 0.42f, 0.55f, 1.0f };
		Color ambientGround = { 0.18f, 0.16f, 0.14f, 1.0f };
		float ambientIntensity = 1.0f;
		bool drawSky = true;
		Color skyZenith = { 0.18f, 0.36f, 0.75f, 1.0f };
		Color skyHorizon = { 0.65f, 0.75f, 0.9f, 1.0f };
		Color fogColor = { 0.6f, 0.7f, 0.85f, 1.0f };
		float fogDensity = 0.0f;
		float fogStart = 10.0f;

		void Reflect(PropertyVisitor& v);
	};

	// 描画設定（エンジン全体）
	struct RenderSettings
	{
		bool shadows = true;
		uint32_t shadowMapSize = 1024;// カスケード 1 つ分（実際のテクスチャはこの 2 倍四方）
		uint32_t shadowCascades = 4;// 1～4。近くほど細かい影になる
		float shadowDistance = 80.0f;
		float cascadeSplitLambda = 0.75f;// 0: 等間隔 / 1: 対数（近くに細かく割り当てる）
		float shadowBias = 0.0005f;
		float shadowStrength = 0.9f;
		bool debugCascades = false;
		bool bloom = true;
		float bloomThreshold = 1.0f;
		float bloomKnee = 0.5f;
		float bloomIntensity = 0.5f;
		float exposure = 1.0f;
		bool tonemap = true;
		float vignette = 0.25f;
		bool frustumCulling = true;
		bool wireframe = false;

		void Reflect(PropertyVisitor& v);
	};

	struct RenderStats
	{
		uint32_t drawCalls = 0;
		uint32_t triangles = 0;
		uint32_t meshesVisible = 0;
		uint32_t meshesCulled = 0;
		uint32_t shadowCasters = 0;
		uint32_t particles = 0;
		uint32_t sprites = 0;
		uint32_t lights = 0;
	};
}
