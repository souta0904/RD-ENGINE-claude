#pragma once
#include "Asset/Model/ModelData.h"
#include <string>

namespace rd::ModelImporter
{
	// assimp で fbx / gltf / obj などを読み、ModelData に変換する（GPU は使わない）
	//  animOnly が true ならスケルトンとアニメーションだけを読む。失敗したら false を返し、error に理由を入れる
	bool Import(const std::string& path, bool animOnly, ModelData& out, std::string& error);
}
