#include "RdPch.h"
#include "Components/Animation/AnimatorController.h"
#include "Components/Animation/Animator.h"
#include "Core/Serialization/Json.h"

namespace rd
{
	void AnimatorController::Reflect(PropertyVisitor& v)
	{
		v.Property("Controller", controllerPath);
	}

	bool AnimatorController::GetBool(const std::string& name) const
	{
		auto it = mValues.find(name);
		return it != mValues.end() && it->second != 0.0f;
	}

	const std::string& AnimatorController::GetCurrentState() const
	{
		static const std::string kEmpty;
		return mCurrent >= 0 ? mStates[size_t(mCurrent)].name : kEmpty;
	}

	float AnimatorController::GetStateLength() const
	{
		return mCurrent >= 0 ? mStates[size_t(mCurrent)].length : 0.0f;
	}

	float AnimatorController::GetNormalizedTime() const
	{
		float length = GetStateLength();
		return length > 0.0f ? mStateTime / length : 1.0f;
	}

	// 基本のレイヤーの状態と遷移を読み、上のレイヤーの状態は Play で使えるように名前だけ足す
	void AnimatorController::Load()
	{
		mLoadedPath = controllerPath;
		mStates.clear();
		mTransitions.clear();
		mDefault = -1;
		mCurrent = -1;
		Json json;
		if (controllerPath.empty() || !LoadJsonFile(controllerPath, json)) { return; }

		for (const auto& p : json.value("Parameters", Json::array()))
		{
			std::string name = p.value("Name", std::string());
			std::string type = p.value("Type", std::string());
			mIsTrigger[name] = type == "Trigger";
			if (type == "Bool") { mValues[name] = p.value("Bool", false) ? 1.0f : 0.0f; }
			else if (type == "Float") { mValues[name] = p.value("Float", 0.0f); }
			else if (type == "Int") { mValues[name] = float(p.value("Int", 0)); }
			else { mValues[name] = 0.0f; }
		}

		const Json& layers = json.value("Layers", Json::array());
		for (size_t l = 0; l < layers.size(); ++l)
		{
			for (const auto& s : layers[l].value("States", Json::array()))
			{
				State state;
				state.name = s.value("Name", std::string());
				state.clip = s.value("Clip", std::string());
				state.speed = s.value("Speed", 1.0f);
				state.length = std::max(s.value("Length", 1.0f), 0.01f);
				state.loop = s.value("Loop", true);
				state.baseLayer = l == 0;
				if (l > 0 && (state.clip.empty() || FindState(state.name) >= 0)) { continue; }
				mStates.push_back(std::move(state));
			}
			if (l > 0) { continue; }
			for (const auto& t : layers[l].value("Transitions", Json::array()))
			{
				Transition tr;
				tr.from = t.value("From", std::string());
				tr.to = t.value("To", std::string());
				tr.hasExitTime = t.value("Has Exit Time", false);
				tr.exitTime = t.value("Exit Time", 0.0f);
				tr.duration = t.value("Duration", 0.0f);
				tr.fixedDuration = t.value("Fixed Duration", true);
				for (const auto& c : t.value("Conditions", Json::array()))
				{
					tr.conditions.push_back({ c.value("Parameter", std::string()), c.value("Mode", std::string()), c.value("Threshold", 0.0f) });
				}
				mTransitions.push_back(std::move(tr));
			}
			mDefault = FindState(layers[l].value("Default", std::string()));
		}
	}

	int AnimatorController::FindState(const std::string& name) const
	{
		for (size_t i = 0; i < mStates.size(); ++i)
		{
			if (mStates[i].name == name) { return int(i); }
		}
		return -1;
	}

	bool AnimatorController::CheckConditions(const Transition& t) const
	{
		for (const auto& c : t.conditions)
		{
			auto it = mValues.find(c.parameter);
			float value = it != mValues.end() ? it->second : 0.0f;
			bool ok = true;
			if (c.mode == "If") { ok = value != 0.0f; }
			else if (c.mode == "IfNot") { ok = value == 0.0f; }
			else if (c.mode == "Greater") { ok = value > c.threshold; }
			else if (c.mode == "Less") { ok = value < c.threshold; }
			else if (c.mode == "Equals") { ok = value == c.threshold; }
			else if (c.mode == "NotEqual") { ok = value != c.threshold; }
			if (!ok) { return false; }
		}
		return true;
	}

	void AnimatorController::Enter(int state, float fade)
	{
		if (state < 0) { return; }
		mCurrent = state;
		mStateTime = 0.0f;
		mFadeLeft = fade;
		const State& s = mStates[size_t(state)];
		if (auto animator = GetComponent<Animator>(); animator && !s.clip.empty())
		{
			animator->loop = s.loop;
			animator->speed = s.speed;
			animator->Play(s.clip, fade, "", true);
		}
	}

	void AnimatorController::Play(const std::string& stateName, float fade)
	{
		if (mLoadedPath != controllerPath) { Load(); }
		int state = FindState(stateName);
		if (state < 0) { return; }
		if (!mStates[size_t(state)].baseLayer)
		{
			if (mCurrent >= 0 && mStates[size_t(mCurrent)].baseLayer) { mReturnState = mCurrent; }
		}
		else
		{
			mReturnState = -1;
		}
		Enter(state, fade);
	}

	// Any State からの遷移を先に調べる（Unity と同じ）。使ったトリガーはその場で戻す
	void AnimatorController::Update(float dt)
	{
		if (mLoadedPath != controllerPath) { Load(); }
		if (mStates.empty()) { return; }
		if (mCurrent < 0) { Enter(mDefault >= 0 ? mDefault : 0, 0.0f); }

		const State& current = mStates[size_t(mCurrent)];
		mStateTime += dt * current.speed;
		mFadeLeft = std::max(0.0f, mFadeLeft - dt);

		if (!current.baseLayer)
		{
			if (mStateTime >= current.length) { Enter(mReturnState >= 0 ? mReturnState : mDefault, 0.15f); }
			return;
		}

		for (int pass = 0; pass < 2; ++pass)
		{
			for (const auto& t : mTransitions)
			{
				bool fromAny = t.from == "Any";
				if ((pass == 0) != fromAny) { continue; }
				if (!fromAny && t.from != current.name) { continue; }
				if (t.conditions.empty() && !t.hasExitTime) { continue; }
				if (t.hasExitTime && GetNormalizedTime() < t.exitTime) { continue; }
				if (!CheckConditions(t)) { continue; }
				int to = FindState(t.to);
				if (to < 0) { continue; }
				for (const auto& c : t.conditions)
				{
					if (mIsTrigger[c.parameter]) { mValues[c.parameter] = 0.0f; }
				}
				float fade = t.fixedDuration ? t.duration : t.duration * mStates[size_t(to)].length;
				Enter(to, fade);
				return;
			}
		}
	}
}
