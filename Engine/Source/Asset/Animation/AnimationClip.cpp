#include "RdPch.h"
#include "Asset/Animation/AnimationClip.h"

namespace rd
{
	namespace
	{
		Vector3 Interpolate(const Vector3& a, const Vector3& b, float t) { return math::Lerp(a, b, t); }
		Quaternion Interpolate(const Quaternion& a, const Quaternion& b, float t) { return Slerp(a, b, t); }

		// time を挟む 2 つのキーを二分探索で見つけて補間する
		template <class T>
		T SampleKeys(const std::vector<Keyframe<T>>& keys, float time, const T& fallback)
		{
			if (keys.empty()) { return fallback; }
			if (keys.size() == 1 || time <= keys.front().time) { return keys.front().value; }
			if (time >= keys.back().time) { return keys.back().value; }
			size_t lo = 0, hi = keys.size() - 1;
			while (hi - lo > 1)
			{
				size_t mid = (lo + hi) / 2;
				if (keys[mid].time <= time) { lo = mid; }
				else { hi = mid; }
			}
			const auto& prev = keys[lo];
			const auto& next = keys[hi];
			float t = (time - prev.time) / std::max(next.time - prev.time, 1.0e-6f);
			return Interpolate(prev.value, next.value, t);
		}
	}

	std::vector<int32_t> AnimationClip::BuildBinding(const Skeleton& skeleton) const
	{
		std::vector<int32_t> binding(channels.size());
		for (size_t i = 0; i < channels.size(); ++i)
		{
			binding[i] = skeleton.Find(channels[i].jointName);
		}
		return binding;
	}

	// 骨の長さの比で、単位の違い（cm と m など）を吸収する
	std::vector<float> AnimationClip::BuildTranslationScale(const Skeleton& target, const Skeleton* source) const
	{
		std::vector<float> scales(channels.size(), 1.0f);
		if (!source || source == &target) { return scales; }
		for (size_t i = 0; i < channels.size(); ++i)
		{
			int32_t tj = target.Find(channels[i].jointName);
			int32_t sj = source->Find(channels[i].jointName);
			if (tj < 0 || sj < 0) { continue; }
			float lt = Length(target.joints[size_t(tj)].bindLocal.translation);
			float ls = Length(source->joints[size_t(sj)].bindLocal.translation);
			scales[i] = ls > 1.0e-5f ? lt / ls : 0.0f;
		}
		return scales;
	}

	void AnimationClip::Sample(float time, std::span<const int32_t> binding, std::span<TransformTRS> outLocal,
		std::span<const float> translationScale) const
	{
		for (size_t i = 0; i < channels.size() && i < binding.size(); ++i)
		{
			int32_t joint = binding[i];
			if (joint < 0 || size_t(joint) >= outLocal.size()) { continue; }
			const auto& ch = channels[i];
			auto& out = outLocal[size_t(joint)];
			float ts = i < translationScale.size() ? translationScale[i] : 1.0f;
			if (ts > 0.0f) { out.translation = SampleKeys(ch.positions, time, out.translation / ts) * ts; }
			out.rotation = SampleKeys(ch.rotations, time, out.rotation);
			out.scale = SampleKeys(ch.scales, time, out.scale);
		}
	}
}
