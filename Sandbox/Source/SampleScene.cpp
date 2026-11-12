#include "SampleComponents.h"
#include "Components/Animator.h"
#include "Components/AudioSource.h"
#include "Components/CameraComponent.h"
#include "Components/Lights.h"
#include "Components/MeshRenderer.h"
#include "Components/ParticleSystem.h"
#include "Components/SpriteRenderer.h"
#include "Physics/Collider.h"
#include "Physics/Rigidbody.h"
#include "Scene/Scene.h"

namespace sample
{
	namespace
	{
		Actor* Primitive(Scene& scene, const std::string& name, const std::string& model, const Vector3& pos,
			const Vector3& scale, const Color& color, float metallic = 0.0f, float roughness = 0.6f)
		{
			Actor* a = scene.CreateActor(name);
			a->GetTransform().SetLocal(pos, Quaternion::kIdentity, scale);
			auto mr = a->AddComponent<MeshRenderer>();
			mr->SetModel(model);
			// プリミティブのマテリアルは 1 つなので先に作っておく
			Material m;
			m.baseColor = color;
			m.metallic = metallic;
			m.roughness = roughness;
			mr->GetMaterials() = { m };
			return a;
		}

		Quaternion Euler(float x, float y, float z)
		{
			return Quaternion::FromEuler({ math::ToRadians(x), math::ToRadians(y), math::ToRadians(z) });
		}
	}

	// サンプルシーンを組み立てる（ファイルがなければ起動時に呼ばれ、保存される）
	void BuildSampleScene(Scene& scene)
	{
		scene.SetName("Sample");

		// 太陽
		Actor* sun = scene.CreateActor("Sun");
		sun->GetTransform().SetLocalRotation(Euler(45.0f, -35.0f, 0.0f));
		auto light = sun->AddComponent<DirectionalLight>();
		light->color = Color(1.0f, 0.95f, 0.85f);
		light->intensity = 3.5f;

		// 地面
		Actor* ground = Primitive(scene, "Ground", "Engine/Plane", Vector3::kZero, Vector3(6, 1, 6), Color(0.35f, 0.42f, 0.35f));
		ground->AddComponent<BoxCollider>()->size = Vector3(10.0f, 0.02f, 10.0f);

		// プレイヤー（物理で動く本体 + 見た目の子アクター）
		Actor* player = scene.CreateActor("Player");
		player->SetTag("Player");
		player->GetTransform().SetLocalPosition({ 0.0f, 0.1f, 0.0f });
		auto rb = player->AddComponent<Rigidbody>();
		rb->freezeRotation = true;
		rb->mass = 70.0f;
		auto capsule = player->AddComponent<CapsuleCollider>();
		capsule->radius = 0.35f;
		capsule->height = 1.8f;
		capsule->center = Vector3(0.0f, 0.9f, 0.0f);
		capsule->friction = 0.0f;
		player->AddComponent<PlayerController>();
		Actor* body = scene.CreateActor("Body", player);
		body->AddComponent<MeshRenderer>()->SetModel("Assets/Sample/Models/Walking/Walking.gltf");
		auto animator = body->AddComponent<Animator>();
		animator->clipSource = "Assets/Sample/Models/Idle/Idle.gltf";
		animator->previewInEditor = true;

		// カメラ
		Actor* camera = scene.CreateActor("Main Camera");
		camera->GetTransform().SetLocalPosition({ 0.0f, 3.5f, -8.0f });
		camera->GetTransform().LookAt({ 0.0f, 1.0f, 0.0f });
		camera->AddComponent<CameraComponent>()->fov = 55.0f;
		camera->AddComponent<FollowCamera>();

		// 物理のデモ: 積み上げた箱と転がる球
		for (int y = 0; y < 4; ++y)
		{
			for (int x = 0; x < 3 - (y % 2); ++x)
			{
				float px = 6.0f + float(x) * 1.05f + (y % 2) * 0.52f;
				Actor* box = Primitive(scene, "Crate", "Engine/Cube", { px, 0.5f + float(y) * 1.0f, 4.0f }, Vector3::kOne,
					Color::FromHSV(0.08f + 0.03f * float(y), 0.6f, 0.8f), 0.0f, 0.5f);
				box->AddComponent<BoxCollider>();
				box->AddComponent<Rigidbody>()->mass = 5.0f;
			}
		}
		for (int i = 0; i < 3; ++i)
		{
			Actor* ball = Primitive(scene, "Ball", "Engine/Sphere", { -5.0f + float(i) * 1.5f, 3.0f + float(i), 5.0f }, Vector3::kOne,
				Color(0.9f, 0.9f, 0.95f), 1.0f, 0.15f + 0.3f * float(i));
			ball->AddComponent<SphereCollider>()->restitution = 0.6f;
			ball->AddComponent<Rigidbody>()->mass = 2.0f;
		}

		// 坂（回転した箱）
		Actor* ramp = Primitive(scene, "Ramp", "Engine/Cube", { -6.0f, 0.8f, -2.0f }, { 4.0f, 0.3f, 6.0f }, Color(0.5f, 0.5f, 0.55f));
		ramp->GetTransform().SetLocalRotation(Euler(-15.0f, 0.0f, 0.0f));
		ramp->AddComponent<BoxCollider>();

		// 収集アイテム（トリガー）
		for (int i = 0; i < 5; ++i)
		{
			float a = math::k2Pi * float(i) / 5.0f;
			Actor* star = scene.CreateActor("Star Piece");
			star->GetTransform().SetLocal({ std::cos(a) * 9.0f, 1.0f, std::sin(a) * 9.0f }, Quaternion::kIdentity, Vector3(0.6f, 0.6f, 0.6f));
			star->AddComponent<MeshRenderer>()->SetModel("Assets/Sample/Models/StarPiece/StarPiece.gltf");
			auto trigger = star->AddComponent<SphereCollider>();
			trigger->isTrigger = true;
			trigger->radius = 1.2f;
			auto rot = star->AddComponent<Rotator>();
			rot->bobAmplitude = 0.25f;
			star->AddComponent<Collectible>();
		}

		// 惑星の飾り
		Actor* planet = scene.CreateActor("Planet");
		planet->GetTransform().SetLocal({ 0.0f, 14.0f, 45.0f }, Euler(15.0f, 0.0f, 10.0f), Vector3(0.4f, 0.4f, 0.4f));
		planet->AddComponent<MeshRenderer>()->SetModel("Assets/Sample/Models/Planet2/Planet2.obj");
		planet->AddComponent<Rotator>()->degreesPerSecond = Vector3(0.0f, 10.0f, 0.0f);

		// 焚き火（点光源 + パーティクル）
		Actor* fire = scene.CreateActor("Campfire");
		fire->GetTransform().SetLocalPosition({ 3.0f, 0.0f, -3.0f });
		auto ps = fire->AddComponent<ParticleSystem>();
		ps->emissionRate = 60.0f;
		ps->coneAngle = 15.0f;
		ps->shapeRadius = 0.3f;
		ps->speedMin = 1.0f;
		ps->speedMax = 2.0f;
		ps->startSize = 0.5f;
		ps->endSize = 0.05f;
		ps->emissive = 4.0f;
		ps->texturePath = "Assets/Sample/Textures/Effect01.png";
		Actor* fireLight = scene.CreateActor("Fire Light", fire);
		fireLight->GetTransform().SetLocalPosition({ 0.0f, 1.0f, 0.0f });
		auto pl = fireLight->AddComponent<PointLight>();
		pl->color = Color(1.0f, 0.55f, 0.2f);
		pl->intensity = 30.0f;
		pl->range = 8.0f;

		// 起動時のジングル
		Actor* bgm = scene.CreateActor("Jingle");
		auto audio = bgm->AddComponent<AudioSource>();
		audio->clipPath = "Assets/Sample/Audio/Open.wav";
		audio->playOnStart = true;
		audio->volume = 0.4f;
		audio->bus = AudioBus::Music;

		// UI（取ったアイテムの数だけハートを表示）
		Actor* hud = scene.CreateActor("HUD");
		hud->GetTransform().SetLocalPosition({ 40.0f, 40.0f, 0.0f });
		hud->AddComponent<ScoreHud>();

		// 環境
		auto& env = scene.GetEnvironment();
		env.fogDensity = 0.008f;
		env.fogStart = 20.0f;

		// まとめて Awake（プロパティを設定してから）
		scene.ForEachActor([](Actor* a) { for (auto& c : a->GetAllComponents()) { c->OnValidate(); } });
	}
}
