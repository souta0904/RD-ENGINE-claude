#pragma once
// エンジンの使い方を示すサンプルのゲーム用コンポーネント
#include "Scene/Actor.h"
#include "Scene/Component.h"

namespace sample
{
	using namespace rd;

	// 剛体で動くキャラクター（カメラ基準の移動・ジャンプ・アニメーション切り替え）
	class PlayerController final : public Component
	{
		RD_COMPONENT(PlayerController)
	public:
		float moveSpeed = 5.0f;
		float jumpSpeed = 6.0f;
		float turnSpeed = 12.0f;
		std::string idleAnimation = "Assets/Sample/Models/Idle/Idle.gltf";
		std::string walkAnimation = "Assets/Sample/Models/Walking/Walking.gltf";

		void Reflect(PropertyVisitor& v) override;
		void Update(float dt) override;

	private:
		bool mWalking = false;
		bool mAnimStarted = false;
	};

	// 対象を追いかける三人称カメラ（右ドラッグ / 右スティックで回り込み）
	class FollowCamera final : public Component
	{
		RD_COMPONENT(FollowCamera)
	public:
		std::string targetName = "Player";
		float distance = 7.0f;
		float height = 2.5f;
		float smoothing = 8.0f;

		void Reflect(PropertyVisitor& v) override;
		void LateUpdate(float dt) override;

	private:
		float mYaw = 0.0f;
	};

	// 一定速度で回転し続ける
	class Rotator final : public Component
	{
		RD_COMPONENT(Rotator)
	public:
		Vector3 degreesPerSecond = { 0.0f, 90.0f, 0.0f };
		float bobAmplitude = 0.0f;// 上下にふわふわ動く量
		void Reflect(PropertyVisitor& v) override;
		void Start() override;
		void Update(float dt) override;

	private:
		Vector3 mBase;
		float mTime = 0.0f;
	};

	// プレイヤーが触れると取れるアイテム（トリガー）
	class Collectible final : public Component
	{
		RD_COMPONENT(Collectible)
	public:
		std::string sound = "Assets/Sample/Audio/ChipGet.wav";
		void Reflect(PropertyVisitor& v) override;
		void Awake() override;
		void OnTriggerEnter(Collider* other) override;

		static inline int sCollected = 0;
	};

	// スコアなどを表示するスプライト UI（取ったアイテム数だけハートを並べる）
	class ScoreHud final : public Component
	{
		RD_COMPONENT(ScoreHud)
	public:
		std::string icon = "Assets/Sample/Textures/Heart.png";
		void Reflect(PropertyVisitor& v) override;
		void OnRender(RenderList& list) override;
	};
}
