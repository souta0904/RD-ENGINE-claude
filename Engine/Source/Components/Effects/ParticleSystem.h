#pragma once
#include "Asset/Asset.h"
#include "Core/Jobs/JobSystem.h"
#include "Math/Math.h"
#include "RHI/Pipeline/PipelineBuilder.h"
#include "RHI/Pipeline/RootSignatureBuilder.h"
#include "RHI/Pipeline/Samplers.h"
#include "Scene/Component/Component.h"
#include <vector>

namespace rd
{
	class Texture;

	// CPU のパーティクル（多いときはジョブで並列に動かす）
	//  emissionRate は 1 秒あたりの数、burstCount は始めに一度に出す数、coneAngle は度
	//  rotationSpeed は度/秒（向きはランダム）、emissive は HDR の明るさ（ブルームに効く）
	class ParticleSystem final : public Component
	{
		RD_COMPONENT(ParticleSystem)
	public:
		enum class Shape { Point, Sphere, Box, Cone };
		enum class Space { World, Local };

		bool playOnStart = true;
		bool loop = true;
		float duration = 5.0f;
		float emissionRate = 20.0f;
		uint32_t burstCount = 0;
		uint32_t maxParticles = 1000;
		Shape shape = Shape::Cone;
		float shapeRadius = 0.5f;
		Vector3 boxSize = Vector3::kOne;
		float coneAngle = 25.0f;
		Space space = Space::World;

		float lifeMin = 1.0f;
		float lifeMax = 2.0f;
		float speedMin = 2.0f;
		float speedMax = 4.0f;
		float startSize = 0.3f;
		float endSize = 0.0f;
		Color startColor = Color(1.0f, 0.8f, 0.4f, 1.0f);
		Color endColor = Color(1.0f, 0.2f, 0.0f, 0.0f);
		float gravityScale = 0.0f;
		float drag = 0.0f;
		float rotationSpeed = 0.0f;

		std::string texturePath;
		BlendMode blend = BlendMode::Additive;
		float emissive = 1.0f;
		bool previewInEditor = true;
		// 時間の倍率を掛けない時間で動かす（時間を止める演出の間も動く）。保存はしない
		bool unscaledTime = false;

		void Reflect(PropertyVisitor& v) override;
		void Start() override;
		void Update(float dt) override;
		void OnEditorUpdate(float dt) override;
		void OnRender(RenderList& list) override;
		void OnDestroy() override { JobSystem::Wait(mJob); }
#if RD_EDITOR
		void OnDrawGizmos(DebugDraw& draw, bool selected) override;
		void OnInspectorGUI() override;
#endif

		void Play();
		void Stop(bool clear = false);
		void Emit(uint32_t count);
		bool IsPlaying() const { return mPlaying; }
		uint32_t GetAliveCount() const { return uint32_t(mParticles.size()); }

	private:
		struct Particle
		{
			Vector3 position;
			Vector3 velocity;
			float age = 0.0f;
			float life = 1.0f;
			float rotation = 0.0f;
			float rotSpeed = 0.0f;
		};
		void Simulate(float dt);
		void SpawnOne(const Matrix4& world);

		std::vector<Particle> mParticles;
		AssetRef<Texture> mTexture;
		JobHandle mJob;
		bool mPlaying = false;
		float mTime = 0.0f;
		float mEmitAccumulator = 0.0f;
	};
}
