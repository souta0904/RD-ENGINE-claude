#include "RdPch.h"
#include "Components/ParticleSystem.h"
#include "Asset/AssetManager.h"
#include "Asset/Texture.h"
#include "Render/DebugDraw.h"
#include "Render/RenderTypes.h"
#include "RHI/GraphicsDevice.h"
#include "Scene/Actor.h"
#include "Scene/Scene.h"
#include "Physics/PhysicsWorld.h"
#if RD_EDITOR
#include <ImGui/imgui.h>
#endif

namespace rd
{
	namespace
	{
		constexpr uint32_t kParallelThreshold = 512;
		constexpr uint32_t kBatchSize = 256;
	}

	void ParticleSystem::Reflect(PropertyVisitor& v)
	{
		if (v.BeginGroup("Emission"))
		{
			v.Property("Play On Start", playOnStart);
			v.Property("Loop", loop);
			v.Property("Duration", duration, { .speed = 0.05f, .min = 0.01f, .max = 1000.0f });
			v.Property("Rate", emissionRate, { .speed = 0.5f, .min = 0.0f, .max = 100000.0f });
			v.Property("Burst", burstCount);
			v.Property("Max Particles", maxParticles);
			v.Property("Shape", shape);
			v.Property("Radius", shapeRadius, { .speed = 0.01f, .min = 0.0f, .max = 1000.0f });
			v.Property("Box Size", boxSize, { .speed = 0.01f });
			v.Property("Cone Angle", coneAngle, { .speed = 0.1f, .min = 0.0f, .max = 180.0f });
			v.Property("Space", space);
			v.EndGroup();
		}
		if (v.BeginGroup("Particle"))
		{
			v.Property("Life Min", lifeMin, { .speed = 0.01f, .min = 0.01f, .max = 100.0f });
			v.Property("Life Max", lifeMax, { .speed = 0.01f, .min = 0.01f, .max = 100.0f });
			v.Property("Speed Min", speedMin, { .speed = 0.05f });
			v.Property("Speed Max", speedMax, { .speed = 0.05f });
			v.Property("Start Size", startSize, { .speed = 0.01f, .min = 0.0f, .max = 100.0f });
			v.Property("End Size", endSize, { .speed = 0.01f, .min = 0.0f, .max = 100.0f });
			v.Property("Start Color", startColor, { .flags = PropertyMeta::HDR });
			v.Property("End Color", endColor, { .flags = PropertyMeta::HDR });
			v.Property("Gravity Scale", gravityScale, { .speed = 0.01f });
			v.Property("Drag", drag, { .speed = 0.01f, .min = 0.0f, .max = 100.0f });
			v.Property("Rotation Speed", rotationSpeed, { .speed = 1.0f });
			v.EndGroup();
		}
		if (v.BeginGroup("Render"))
		{
			v.Property("Texture", texturePath, { .flags = PropertyMeta::AssetTexture });
			v.Property("Blend", blend);
			v.Property("Emissive", emissive, { .speed = 0.05f, .min = 0.0f, .max = 100.0f });
			v.Property("Preview In Editor", previewInEditor);
			v.EndGroup();
		}
	}

	void ParticleSystem::Start()
	{
		mParticles.clear();
		if (playOnStart) { Play(); }
	}

	void ParticleSystem::Play()
	{
		mPlaying = true;
		mTime = 0.0f;
		mEmitAccumulator = 0.0f;
		if (burstCount > 0) { Emit(burstCount); }
	}

	void ParticleSystem::Stop(bool clear)
	{
		mPlaying = false;
		if (clear)
		{
			JobSystem::Wait(mJob);
			mParticles.clear();
		}
	}

	void ParticleSystem::Emit(uint32_t count)
	{
		JobSystem::Wait(mJob);
		const Matrix4& world = GetTransform().GetWorldMatrix();
		for (uint32_t i = 0; i < count && mParticles.size() < maxParticles; ++i) { SpawnOne(world); }
	}

	void ParticleSystem::SpawnOne(const Matrix4& world)
	{
		Particle p;
		Vector3 localPos;
		Vector3 dir = Vector3::kUnitY;
		switch (shape)
		{
		case Shape::Point:
			dir = Random::OnUnitSphere();
			break;
		case Shape::Sphere:
			dir = Random::OnUnitSphere();
			localPos = dir * shapeRadius * Random::Value();
			break;
		case Shape::Box:
			localPos = Random::Range(boxSize * -0.5f, boxSize * 0.5f);
			dir = Vector3::kUnitY;
			break;
		case Shape::Cone:
		{
			// 円錐の中のランダムな方向（+Y 軸まわり）
			float angle = math::ToRadians(coneAngle) * std::sqrt(Random::Value());
			float phi = Random::Range(0.0f, math::k2Pi);
			dir = Vector3(std::sin(angle) * std::cos(phi), std::cos(angle), std::sin(angle) * std::sin(phi));
			localPos = Vector3(std::cos(phi), 0.0f, std::sin(phi)) * shapeRadius * Random::Value();
			break;
		}
		}
		float speed = Random::Range(speedMin, speedMax);
		if (space == Space::World)
		{
			p.position = TransformPoint(localPos, world);
			p.velocity = Normalize(TransformVector(dir, world)) * speed;
		}
		else
		{
			p.position = localPos;
			p.velocity = dir * speed;
		}
		p.life = Random::Range(lifeMin, std::max(lifeMin, lifeMax));
		p.rotation = Random::Range(0.0f, math::k2Pi);
		p.rotSpeed = math::ToRadians(rotationSpeed) * Random::Range(-1.0f, 1.0f);
		mParticles.push_back(p);
	}

	void ParticleSystem::Update(float dt)
	{
		Simulate(dt);
	}

	void ParticleSystem::OnEditorUpdate(float dt)
	{
		if (previewInEditor)
		{
			if (!mPlaying && mParticles.empty()) { Play(); }
			Simulate(dt);
		}
		else if (!mParticles.empty())
		{
			Stop(true);
		}
	}

	void ParticleSystem::Simulate(float dt)
	{
		JobSystem::Wait(mJob);

		// 寿命切れを削除
		mParticles.erase(std::remove_if(mParticles.begin(), mParticles.end(),
			[](const Particle& p) { return p.age >= p.life; }), mParticles.end());

		// 放出
		if (mPlaying)
		{
			mTime += dt;
			const Matrix4& world = GetTransform().GetWorldMatrix();
			mEmitAccumulator += emissionRate * dt;
			while (mEmitAccumulator >= 1.0f && mParticles.size() < maxParticles)
			{
				SpawnOne(world);
				mEmitAccumulator -= 1.0f;
			}
			mEmitAccumulator = std::min(mEmitAccumulator, 1.0f);
			if (mTime >= duration)
			{
				if (loop) { mTime = 0.0f; if (burstCount > 0) { Emit(burstCount); } }
				else { mPlaying = false; }
			}
		}

		// 移動（多ければ並列）
		Vector3 gravity = GetScene()->GetPhysics().gravity * gravityScale;
		float dragFactor = 1.0f / (1.0f + drag * dt);
		auto update = [this, dt, gravity, dragFactor](uint32_t begin, uint32_t end)
			{
				for (uint32_t i = begin; i < end; ++i)
				{
					Particle& p = mParticles[i];
					p.velocity = (p.velocity + gravity * dt) * dragFactor;
					p.position += p.velocity * dt;
					p.rotation += p.rotSpeed * dt;
					p.age += dt;
				}
			};
		uint32_t count = uint32_t(mParticles.size());
		if (count >= kParallelThreshold)
		{
			mJob = JobSystem::ParallelFor(count, kBatchSize, update);
		}
		else
		{
			update(0, count);
		}
	}

	void ParticleSystem::OnRender(RenderList& list)
	{
		JobSystem::Wait(mJob);
		if (mParticles.empty()) { return; }
		if (!texturePath.empty() && (!mTexture || mTexture->GetPath() != texturePath))
		{
			mTexture = AssetManager::Get().LoadTexture(texturePath);
		}

		auto& device = GraphicsDevice::Get();
		auto alloc = device.AllocateDynamic(sizeof(GpuParticle) * mParticles.size(), 16);
		auto* gpu = static_cast<GpuParticle*>(alloc.cpu);
		const Matrix4& world = GetTransform().GetWorldMatrix();
		bool local = space == Space::Local;
		AABB bounds;
		for (size_t i = 0; i < mParticles.size(); ++i)
		{
			const Particle& p = mParticles[i];
			float t = math::Saturate(p.age / p.life);
			GpuParticle& g = gpu[i];
			g.position = local ? TransformPoint(p.position, world) : p.position;
			g.size = math::Lerp(startSize, endSize, t);
			g.color = math::Lerp(startColor, endColor, t);
			g.rotation = p.rotation;
			bounds.Expand(g.position);
		}

		ParticleDrawItem item;
		item.instances = alloc.gpu;
		item.count = uint32_t(mParticles.size());
		item.texture = mTexture.get();
		item.blend = blend == BlendMode::Additive ? BlendMode::Additive : BlendMode::Alpha;
		item.color = Color(emissive, emissive, emissive, 1.0f);
		item.center = bounds.GetCenter();
		list.particles.push_back(item);
	}

#if RD_EDITOR
	void ParticleSystem::OnDrawGizmos(DebugDraw& draw, bool selected)
	{
		if (!selected) { return; }
		const Transform& t = GetTransform();
		Color c(0.6f, 0.8f, 1.0f, 0.8f);
		switch (shape)
		{
		case Shape::Sphere: draw.Sphere(t.GetPosition(), shapeRadius, c); break;
		case Shape::Box: draw.Box(Matrix4::CreateScale(boxSize) * t.GetWorldMatrix(), c); break;
		case Shape::Cone: draw.Cone(t.GetPosition(), t.GetUp(), 1.0f, math::ToRadians(coneAngle), c); break;
		default: break;
		}
	}

	void ParticleSystem::OnInspectorGUI()
	{
		if (ImGui::Button(mPlaying ? "Stop" : "Play")) { mPlaying ? Stop() : Play(); }
		ImGui::SameLine();
		if (ImGui::Button("Clear")) { Stop(true); }
		ImGui::SameLine();
		ImGui::Text("Alive: %u", GetAliveCount());
	}
#endif
}
