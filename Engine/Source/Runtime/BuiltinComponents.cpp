#include "RdPch.h"
#include "BuiltinComponents.h"
#include "Components/Animator.h"
#include "Components/AudioSource.h"
#include "Components/CameraComponent.h"
#include "Components/Lights.h"
#include "Components/MeshRenderer.h"
#include "Components/ParticleSystem.h"
#include "Components/SpriteRenderer.h"
#include "Physics/Collider.h"
#include "Physics/Rigidbody.h"
#include "Scene/ComponentRegistry.h"

namespace rd
{
	void RegisterBuiltinComponents()
	{
		ComponentRegistry::Register<MeshRenderer>("Rendering");
		ComponentRegistry::Register<SpriteRenderer>("Rendering");
		ComponentRegistry::Register<ParticleSystem>("Rendering");
		ComponentRegistry::Register<CameraComponent>("Rendering");
		ComponentRegistry::Register<DirectionalLight>("Light");
		ComponentRegistry::Register<PointLight>("Light");
		ComponentRegistry::Register<SpotLight>("Light");
		ComponentRegistry::Register<Animator>("Animation");
		ComponentRegistry::Register<Rigidbody>("Physics");
		ComponentRegistry::Register<SphereCollider>("Physics");
		ComponentRegistry::Register<BoxCollider>("Physics");
		ComponentRegistry::Register<CapsuleCollider>("Physics");
		ComponentRegistry::Register<MeshCollider>("Physics");
		ComponentRegistry::Register<AudioSource>("Audio");
	}
}
