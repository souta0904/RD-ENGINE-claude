#pragma once
#include "Asset/Animation/TransformTRS.h"
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace rd
{
	// スケルトンの関節（親は必ず自分より前に並ぶ。inverseBind はメッシュ空間 → 関節空間、ボーンでなければ単位行列）
	struct Joint
	{
		std::string name;
		int32_t parent = -1;
		TransformTRS bindLocal;
		Matrix4 inverseBind;
	};

	// スケルトン（モデルのノード階層）
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

	// 関節のローカル姿勢から、ワールド行列とスキニング用の行列を求める
	void ComputeSkinningPalette(const Skeleton& skeleton, std::span<const TransformTRS> local,
		std::vector<Matrix4>& outGlobal, std::vector<Matrix4>& outPalette);
}
