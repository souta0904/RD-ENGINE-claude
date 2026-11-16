#include "RdPch.h"
#include "Asset/Animation/Skeleton.h"

namespace rd
{
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
			// 行ベクトル規約なので「子のローカル * 親のグローバル」（親は先に計算済み）
			outGlobal[i] = joint.parent >= 0 ? localMat * outGlobal[size_t(joint.parent)] : localMat;
			outPalette[i] = joint.inverseBind * outGlobal[i];
		}
	}
}
