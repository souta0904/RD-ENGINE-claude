#pragma once
#include "Asset/Model/MeshVertex.h"
#include <vector>

// 組み込みのプリミティブ（モデルのキー "Engine/Cube", "Engine/Sphere", "Engine/Plane", "Engine/Capsule"）
namespace rd::Primitives
{
	void Cube(std::vector<MeshVertex>& v, std::vector<uint32_t>& i);
	void Sphere(std::vector<MeshVertex>& v, std::vector<uint32_t>& i, uint32_t segments = 32, uint32_t rings = 16);
	// UV は 1m ごとに 1 回繰り返す
	void Plane(std::vector<MeshVertex>& v, std::vector<uint32_t>& i, float size = 10.0f);
	void Capsule(std::vector<MeshVertex>& v, std::vector<uint32_t>& i, float radius = 0.5f, float height = 2.0f);
}
