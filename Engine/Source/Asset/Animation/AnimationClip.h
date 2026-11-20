#pragma once
#include "Asset/Animation/Skeleton.h"

namespace rd
{
	template <class T>
	struct Keyframe
	{
		float time;
		T value;
	};

	// 1 つの関節のキーの並び
	struct AnimationChannel
	{
		std::string jointName;
		std::vector<Keyframe<Vector3>> positions;
		std::vector<Keyframe<Quaternion>> rotations;
		std::vector<Keyframe<Vector3>> scales;
	};

	// アニメーションのクリップ（duration は秒）
	class RD_API AnimationClip
	{
	public:
		std::string name;
		float duration = 0.0f;
		std::vector<AnimationChannel> channels;

		// チャンネル → 関節の番号の対応表（スケルトンごとに 1 回作る）
		std::vector<int32_t> BuildBinding(const Skeleton& skeleton) const;
		// time のローカル姿勢を求める（チャンネルのない関節は触らない）
		//  translationScale: チャンネルごとの平行移動の倍率（別のスケルトンのクリップを使うとき。0 なら平行移動を使わない）
		void Sample(float time, std::span<const int32_t> binding, std::span<TransformTRS> outLocal,
			std::span<const float> translationScale = {}) const;
		// 別のスケルトン用に作られたクリップを使うときの平行移動の倍率
		std::vector<float> BuildTranslationScale(const Skeleton& target, const Skeleton* source) const;
	};
}
