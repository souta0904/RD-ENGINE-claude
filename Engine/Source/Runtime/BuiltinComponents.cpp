#include "RdPch.h"
#include "Runtime/BuiltinComponents.h"
#include "Components/Animation/Animator.h"
#include "Components/Audio/AudioSource.h"
#include "Components/Effects/ParticleSystem.h"
#include "Components/Lighting/DirectionalLight.h"
#include "Components/Lighting/PointLight.h"
#include "Components/Lighting/SpotLight.h"
#include "Components/Rendering/CameraComponent.h"
#include "Components/Rendering/MeshRenderer.h"
#include "Components/Rendering/SpriteRenderer.h"
#include "Physics/Colliders/BoxCollider.h"
#include "Physics/Colliders/CapsuleCollider.h"
#include "Physics/Colliders/MeshCollider.h"
#include "Physics/Colliders/SphereCollider.h"
#include "Physics/Rigidbody/Rigidbody.h"
#include "Scene/Component/ComponentRegistry.h"

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
