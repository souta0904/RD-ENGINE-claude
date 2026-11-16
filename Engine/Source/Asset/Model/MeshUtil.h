#pragma once
#include "Asset/Model/MeshVertex.h"
#include <vector>

// メッシュの頂点データの後処理
namespace rd::MeshUtil
{
	// 三角形の向きを法線にそろえる（左手系 + D3D の既定のカリングで表になるように）
	void FixWinding(const std::vector<MeshVertex>& vertices, std::vector<uint32_t>& indices);
	// UV から接線を求める
	void ComputeTangents(std::vector<MeshVertex>& vertices, const std::vector<uint32_t>& indices);
}
