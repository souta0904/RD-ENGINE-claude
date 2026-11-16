#pragma once
#include "Math/Geometry/AABB.h"
#include "Math/Vector4.h"
#include "RHI/Pipeline/PipelineBuilder.h"

namespace rd
{
	class Model;
	class Texture;
	struct Material;

	// メッシュ 1 つの描画（palette はスキニング行列。0 なら静的）
	struct MeshDrawItem
	{
		const Model* model = nullptr;
		uint32_t subMesh = 0;
		Matrix4 world;
		const Material* material = nullptr;
		D3D12_GPU_VIRTUAL_ADDRESS palette = 0;
		AABB worldBounds;
		Color tint = Color::kWhite;
		float sortKey = 0.0f;
	};

	// パーティクルの描画（instances は GpuParticle の配列）
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

	// スプライトの描画（position は左上が原点のスクリーン座標（ピクセル）、uvRect は u0, v0, u1, v1）
	struct SpriteDrawItem
	{
		const Texture* texture = nullptr;
		Vector2 position;
		Vector2 size = { 100.0f, 100.0f };
		Vector2 pivot = { 0.5f, 0.5f };
		float rotation = 0.0f;
		Color color = Color::kWhite;
		Vector4 uvRect = { 0.0f, 0.0f, 1.0f, 1.0f };
		int32_t layer = 0;
	};
}
