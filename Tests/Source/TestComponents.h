#pragma once
#include "Scene/Actor.h"
#include "Scene/ObjectRef.h"
#include <string>
#include <vector>

// テスト用のコンポーネント
namespace rdtest
{
	enum class Mood { Calm, Angry, Sleepy };

	// 呼ばれたコールバックを記録する
	class Recorder final : public rd::Component
	{
		RD_COMPONENT(Recorder)
	public:
		int value = 0;
		std::vector<std::string>* log = nullptr;// テストが渡す（保存されない）

		void Reflect(rd::PropertyVisitor& v) override { v.Property("Value", value); }
		void Awake() override { Push("Awake"); }
		void Start() override { Push("Start"); }
		void Update(float) override { Push("Update"); }
		void FixedUpdate(float) override { Push("FixedUpdate"); }
		void OnDestroy() override { Push("OnDestroy"); }

	private:
		void Push(const char* what)
		{
			if (log) { log->push_back(GetActor()->GetName() + "." + what + "." + std::to_string(value)); }
		}
	};

	// コールバックを何も上書きしない（呼び出しリストに入らない）
	class Passive final : public rd::Component
	{
		RD_COMPONENT(Passive)
	};

	// LateUpdate だけ
	class LateOnly final : public rd::Component
	{
		RD_COMPONENT(LateOnly)
	public:
		int calls = 0;
		void LateUpdate(float) override { ++calls; }
	};

	// すべての種類のプロパティと参照
	class AllProps final : public rd::Component
	{
		RD_COMPONENT(AllProps)
	public:
		bool flag = false;
		int32_t count = 0;
		uint32_t mask = 0;
		float speed = 0.0f;
		rd::Vector3 offset;
		rd::Quaternion rotation;
		rd::Color tint = rd::Color::kWhite;
		std::string label;
		Mood mood = Mood::Calm;
		rd::ActorRef target;
		rd::ComponentRef<Recorder> recorder;

		void Reflect(rd::PropertyVisitor& v) override
		{
			v.Property("Flag", flag);
			v.Property("Count", count);
			v.Property("Mask", mask);
			v.Property("Speed", speed);
			if (v.BeginGroup("Pose"))
			{
				v.Property("Offset", offset);
				v.Property("Rotation", rotation);
				v.EndGroup();
			}
			v.Property("Tint", tint);
			v.Property("Label", label);
			v.Property("Mood", mood);
			v.Property("Target", target);
			v.Property("Recorder", recorder);
		}
	};
}
