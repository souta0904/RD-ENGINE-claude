#pragma once
#include "Scene/Component/Component.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace rd
{
	class Animator;

	// Unity の AnimatorController の状態遷移を、書き出した JSON から動かす（同じアクターの Animator でクリップを流す）
	//  パラメーター（Bool / Trigger / Float / Int）の条件と終了時間で、基本のレイヤーの状態を切り替える。
	//  上のレイヤーにしかない状態（食べるなど）を Play で指定したときは、そのクリップを 1 回流してから元の状態に戻る。
	class AnimatorController final : public Component
	{
		RD_COMPONENT(AnimatorController)
	public:
		std::string controllerPath;

		void Reflect(PropertyVisitor& v) override;
		void Update(float dt) override;

		void SetBool(const std::string& name, bool value) { mValues[name] = value ? 1.0f : 0.0f; }
		bool GetBool(const std::string& name) const;
		void SetFloat(const std::string& name, float value) { mValues[name] = value; }
		void SetTrigger(const std::string& name) { mValues[name] = 1.0f; }
		void ResetTrigger(const std::string& name) { mValues[name] = 0.0f; }
		void Play(const std::string& stateName, float fade = 0.0f);

		const std::string& GetCurrentState() const;
		// 状態に入ってからの時間をクリップの長さで割った値（ループしても 1 を超えて増える）
		float GetNormalizedTime() const;
		float GetStateLength() const;
		bool IsInTransition() const { return mFadeLeft > 0.0f; }

	private:
		struct Condition
		{
			std::string parameter;
			std::string mode;
			float threshold = 0.0f;
		};
		struct Transition
		{
			std::string from;
			std::string to;
			bool hasExitTime = false;
			float exitTime = 0.0f;
			float duration = 0.0f;
			bool fixedDuration = true;
			std::vector<Condition> conditions;
		};
		struct State
		{
			std::string name;
			std::string clip;
			float speed = 1.0f;
			float length = 1.0f;
			bool loop = true;
			bool baseLayer = true;
		};

		void Load();
		int FindState(const std::string& name) const;
		bool CheckConditions(const Transition& t) const;
		void Enter(int state, float fade);

		std::string mLoadedPath;
		std::vector<State> mStates;
		std::vector<Transition> mTransitions;
		std::unordered_map<std::string, float> mValues;
		std::unordered_map<std::string, bool> mIsTrigger;
		int mDefault = -1;
		int mCurrent = -1;
		int mReturnState = -1;
		float mStateTime = 0.0f;
		float mFadeLeft = 0.0f;
	};
}
