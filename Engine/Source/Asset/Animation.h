#pragma once
#include "Math/Math.h"
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace rd
{
	// 平行移動・回転・拡縮
	struct TransformTRS
	{
		Vector3 translation;
		Quaternion rotation;
		Vector3 scale = Vector3::kOne;

		Matrix4 ToMatrix() const { return Matrix4::CreateAffine(scale, rotation, translation); }
		static TransformTRS Blend(const TransformTRS& a, const TransformTRS& b, float t)
		{
			return { math::Lerp(a.translation, b.translation, t), Slerp(a.rotation, b.rotation, t), math::Lerp(a.scale, b.scale, t) };
		}
	};

	struct Joint
	{
		std::string name;
		int32_t parent = -1;// 親は必ず自分より前に並ぶ
		TransformTRS bindLocal;// バインドポーズのローカル変換
		Matrix4 inverseBind;// メッシュ空間 -> ジョイント空間（ボーンでなければ単位行列）
	};

	// スケルトン（ノード階層）
	struct Skeleton
	{
		std::vector<Joint> joints;
		std::unordered_map<std::string, int32_t> nameToIndex;

		int32_t Find(const std::string& name) const
		{
			auto it = nameToIndex.find(name);
			return it != nameToIndex.end() ? it->second : -1;
		}
	};

	template <class T>
	struct Keyframe
	{
		float time;// 秒
		T value;
	};

	struct AnimationChannel
	{
		std::string jointName;
		std::vector<Keyframe<Vector3>> positions;
		std::vector<Keyframe<Quaternion>> rotations;
		std::vector<Keyframe<Vector3>> scales;
	};

	// アニメーションクリップ
	class AnimationClip
	{
	public:
		std::string name;
		float duration = 0.0f;// 秒
		std::vector<AnimationChannel> channels;

		// チャンネル -> ジョイント番号の対応表を作る（スケルトンごとに 1 回）
		std::vector<int32_t> BuildBinding(const Skeleton& skeleton) const;
		// 指定時刻のローカル姿勢を求める（チャンネルのないジョイントは触らない）
		//  translationScale: チャンネルごとの平行移動の倍率（別スケルトンからの流用用、0 なら平行移動を使わない）
		void Sample(float time, std::span<const int32_t> binding, std::span<TransformTRS> outLocal,
			std::span<const float> translationScale = {}) const;
		// 別のスケルトン用に作られたクリップを流用するときの平行移動の倍率
		std::vector<float> BuildTranslationScale(const Skeleton& target, const Skeleton* source) const;
	};

	// ローカル姿勢からスキニング行列パレットを計算
	void ComputeSkinningPalette(const Skeleton& skeleton, std::span<const TransformTRS> local,
		std::vector<Matrix4>& outGlobal, std::vector<Matrix4>& outPalette);
}
