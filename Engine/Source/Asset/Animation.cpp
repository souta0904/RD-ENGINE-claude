#include "RdPch.h"
#include "Asset/Animation.h"

namespace rd
{
	namespace
	{
		// time を挟む 2 つのキーを二分探索して補間する
		template <class T, class Interp>
		T SampleKeys(const std::vector<Keyframe<T>>& keys, float time, const T& fallback, Interp interp)
		{
			if (keys.empty()) { return fallback; }
			if (keys.size() == 1 || time <= keys.front().time) { return keys.front().value; }
			if (time >= keys.back().time) { return keys.back().value; }
			auto it = std::upper_bound(keys.begin(), keys.end(), time,
				[](float t, const Keyframe<T>& k) { return t < k.time; });
			const auto& next = *it;
			const auto& prev = *(it - 1);
			float t = (time - prev.time) / std::max(next.time - prev.time, 1.0e-6f);
			return interp(prev.value, next.value, t);
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
			// 骨の長さの比で単位の違い（cm と m など）を吸収する
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
			if (ts > 0.0f)
			{
				out.translation = SampleKeys(ch.positions, time, out.translation / ts,
					[](const Vector3& a, const Vector3& b, float t) { return math::Lerp(a, b, t); }) * ts;
			}
			out.rotation = SampleKeys(ch.rotations, time, out.rotation,
				[](const Quaternion& a, const Quaternion& b, float t) { return Slerp(a, b, t); });
			out.scale = SampleKeys(ch.scales, time, out.scale,
				[](const Vector3& a, const Vector3& b, float t) { return math::Lerp(a, b, t); });
		}
	}

	void ComputeSkinningPalette(const Skeleton& skeleton, std::span<const TransformTRS> local,
		std::vector<Matrix4>& outGlobal, std::vector<Matrix4>& outPalette)
	{
		size_t count = skeleton.joints.size();
		outGlobal.resize(count);
		outPalette.resize(count);
		for (size_t i = 0; i < count; ++i)
		{
			const Joint& joint = skeleton.joints[i];
			Matrix4 localMat = local[i].ToMatrix();
			// 行ベクトル規約なので「子のローカル * 親のグローバル」
			outGlobal[i] = joint.parent >= 0 ? localMat * outGlobal[size_t(joint.parent)] : localMat;
			outPalette[i] = joint.inverseBind * outGlobal[i];
		}
	}
}
