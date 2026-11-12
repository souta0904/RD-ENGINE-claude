#include "SampleComponents.h"
#include "Asset/AssetManager.h"
#include "Asset/Texture.h"
#include "Audio/AudioSystem.h"
#include "Components/Animator.h"
#include "Components/CameraComponent.h"
#include "Core/Log.h"
#include "Physics/Collider.h"
#include "Physics/PhysicsWorld.h"
#include "Physics/Rigidbody.h"
#include "Platform/Input.h"
#include "Render/RenderTypes.h"
#include "Scene/ComponentRegistry.h"
#include "Scene/Scene.h"

namespace sample
{
	// .cpp に書くだけでエディタの「Add Component」に出てくる
	RD_REGISTER_COMPONENT(PlayerController, "Sample");
	RD_REGISTER_COMPONENT(FollowCamera, "Sample");
	RD_REGISTER_COMPONENT(Rotator, "Sample");
	RD_REGISTER_COMPONENT(Collectible, "Sample");
	RD_REGISTER_COMPONENT(ScoreHud, "Sample");

	// ==================================================
	// PlayerController
	// ==================================================
	void PlayerController::Reflect(PropertyVisitor& v)
	{
		v.Property("Move Speed", moveSpeed, { .speed = 0.05f, .min = 0.0f, .max = 100.0f });
		v.Property("Jump Speed", jumpSpeed, { .speed = 0.05f, .min = 0.0f, .max = 100.0f });
		v.Property("Turn Speed", turnSpeed, { .speed = 0.1f, .min = 0.0f, .max = 100.0f });
		v.Property("Idle Animation", idleAnimation, { .flags = PropertyMeta::AssetModel });
		v.Property("Walk Animation", walkAnimation, { .flags = PropertyMeta::AssetModel });
	}

	void PlayerController::Update(float dt)
	{
		auto rb = GetComponent<Rigidbody>();
		if (!rb) { return; }

		// 入力（キーボード + ゲームパッド）
		Vector2 input = Input::GetLeftStick();
		if (Input::GetKey(Key::W) || Input::GetKey(Key::Up)) { input.y += 1.0f; }
		if (Input::GetKey(Key::S) || Input::GetKey(Key::Down)) { input.y -= 1.0f; }
		if (Input::GetKey(Key::D) || Input::GetKey(Key::Right)) { input.x += 1.0f; }
		if (Input::GetKey(Key::A) || Input::GetKey(Key::Left)) { input.x -= 1.0f; }
		if (LengthSq(input) > 1.0f) { input = Normalize(input); }

		// カメラの向きを基準に移動方向を決める
		Vector3 forward = Vector3::kUnitZ, right = Vector3::kUnitX;
		if (auto cam = GetScene()->GetMainCamera())
		{
			forward = Normalize(ProjectOnPlane(cam->GetTransform().GetForward(), Vector3::kUnitY));
			right = Cross(Vector3::kUnitY, forward);
		}
		Vector3 move = (forward * input.y + right * input.x) * moveSpeed;
		rb->velocity.x = move.x;
		rb->velocity.z = move.z;

		if (rb->IsGrounded() && (Input::GetKeyDown(Key::Space) || Input::GetGamepadButtonDown(GamepadButton::A)))
		{
			rb->velocity.y = jumpSpeed;
		}

		// 進行方向へ向く
		if (LengthSq(move) > 0.01f)
		{
			Quaternion target = Quaternion::LookRotation(move);
			GetTransform().SetRotation(Slerp(GetTransform().GetRotation(), target, 1.0f - std::exp(-turnSpeed * dt)));
		}

		// アニメーションの切り替え（状態が変わったときだけ）
		bool walking = LengthSq(move) > 0.1f;
		if (auto animator = GetActor()->GetComponentInChildren<Animator>(); animator && (walking != mWalking || !mAnimStarted))
		{
			animator->Play("", mAnimStarted ? 0.25f : 0.0f, walking ? walkAnimation : idleAnimation);
			mWalking = walking;
			mAnimStarted = true;
		}
	}

	// ==================================================
	// FollowCamera
	// ==================================================
	void FollowCamera::Reflect(PropertyVisitor& v)
	{
		v.Property("Target", targetName);
		v.Property("Distance", distance, { .speed = 0.05f, .min = 0.5f, .max = 100.0f });
		v.Property("Height", height, { .speed = 0.05f });
		v.Property("Smoothing", smoothing, { .speed = 0.1f, .min = 0.0f, .max = 50.0f });
	}

	void FollowCamera::LateUpdate(float dt)
	{
		Actor* target = GetScene()->FindActorByName(targetName);
		if (!target) { return; }

		// 回り込み
		if (Input::GetMouseButton(MouseButton::Right)) { mYaw += Input::GetMouseDelta().x * 0.005f; }
		mYaw += Input::GetRightStick().x * 2.5f * dt;

		Vector3 focus = target->GetTransform().GetPosition() + Vector3(0.0f, 1.2f, 0.0f);
		Vector3 offset = Vector3(std::sin(mYaw), 0.0f, -std::cos(mYaw)) * distance + Vector3(0.0f, height, 0.0f);
		Vector3 desired = focus + offset;

		// 壁にめり込まないようにレイを飛ばす
		Vector3 dir = desired - focus;
		float len = Length(dir);
		RaycastHit hit;
		if (len > 0.01f && GetScene()->GetPhysics().Raycast({ focus, dir / len }, len, hit) && hit.collider->GetActor() != target)
		{
			desired = hit.point - dir / len * 0.3f;
		}

		Transform& t = GetTransform();
		float k = smoothing > 0.0f ? 1.0f - std::exp(-smoothing * dt) : 1.0f;
		t.SetPosition(math::Lerp(t.GetPosition(), desired, k));
		t.LookAt(focus);
	}

	// ==================================================
	// Rotator
	// ==================================================
	void Rotator::Reflect(PropertyVisitor& v)
	{
		v.Property("Degrees Per Second", degreesPerSecond);
		v.Property("Bob Amplitude", bobAmplitude, { .speed = 0.01f });
	}

	void Rotator::Start()
	{
		mBase = GetTransform().GetLocalPosition();
	}

	void Rotator::Update(float dt)
	{
		mTime += dt;
		Vector3 r = degreesPerSecond * dt;
		GetTransform().Rotate(Quaternion::FromEuler({ math::ToRadians(r.x), math::ToRadians(r.y), math::ToRadians(r.z) }), false);
		if (bobAmplitude != 0.0f)
		{
			GetTransform().SetLocalPosition(mBase + Vector3(0.0f, std::sin(mTime * 2.0f) * bobAmplitude, 0.0f));
		}
	}

	// ==================================================
	// Collectible
	// ==================================================
	void Collectible::Reflect(PropertyVisitor& v)
	{
		v.Property("Sound", sound, { .flags = PropertyMeta::AssetAudio });
	}

	void Collectible::Awake()
	{
		AssetManager::Get().LoadAudio(sound);// 先読み
		sCollected = 0;
	}

	void Collectible::OnTriggerEnter(Collider* other)
	{
		if (other->GetActor()->GetTag() != "Player") { return; }
		AudioSystem::Get().Play(AssetManager::Get().LoadAudio(sound));
		++sCollected;
		Log::Info("Collected {} item(s)", sCollected);
		GetActor()->Destroy();
	}

	// ==================================================
	// ScoreHud
	// ==================================================
	void ScoreHud::Reflect(PropertyVisitor& v)
	{
		v.Property("Icon", icon, { .flags = PropertyMeta::AssetTexture });
	}

	void ScoreHud::OnRender(RenderList& list)
	{
		auto tex = AssetManager::Get().LoadTexture(icon);
		Vector3 origin = GetTransform().GetPosition();
		for (int i = 0; i < Collectible::sCollected; ++i)
		{
			SpriteDrawItem s;
			s.texture = tex.get();
			s.position = Vector2(origin.x + 48.0f * float(i), origin.y);
			s.size = Vector2(40.0f, 40.0f);
			s.layer = 10;
			list.sprites.push_back(s);
		}
	}
}
