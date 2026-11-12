#pragma once
#include "Asset/Asset.h"
#include "Core/JobSystem.h"
#include "Math/Math.h"
#include "RHI/Pipeline.h"
#include "Scene/Component.h"
#include <vector>

namespace rd
{
	class Texture;

	// CPU パーティクル（多いときはジョブシステムで並列更新）
	class ParticleSystem final : public Component
	{
		RD_COMPONENT(ParticleSystem)
	public:
		enum class Shape { Point, Sphere, Box, Cone };
		enum class Space { World, Local };

		// 放出
		bool playOnStart = true;
		bool loop = true;
		float duration = 5.0f;
		float emissionRate = 20.0f;// 毎秒
		uint32_t burstCount = 0;// 開始時に一度に出す数
		uint32_t maxParticles = 1000;
		Shape shape = Shape::Cone;
		float shapeRadius = 0.5f;
		Vector3 boxSize = Vector3::kOne;
		float coneAngle = 25.0f;// 度
		Space space = Space::World;
		// 粒子
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
		float rotationSpeed = 0.0f;// 度/秒（ランダムな向き）
		// 描画
		std::string texturePath;
		BlendMode blend = BlendMode::Additive;
		float emissive = 1.0f;// HDR の明るさ（ブルームに効く）
		bool previewInEditor = true;

		void Reflect(PropertyVisitor& v) override;
		void Start() override;
		void Update(float dt) override;
		void OnEditorUpdate(float dt) override;
		void OnRender(RenderList& list) override;
		void OnDestroy() override { JobSystem::Wait(mJob); }
		void OnDrawGizmos(DebugDraw& draw, bool selected) override;
		void OnInspectorGUI() override;

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
