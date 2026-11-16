#pragma once
#include <cstdint>

namespace rd
{
	// プロパティの付加情報（エディタでの表示に使う。min == max なら範囲なし）
	struct PropertyMeta
	{
		enum Flags : uint32_t
		{
			None = 0,
			Angle = 1 << 0,
			HDR = 1 << 1,
			ReadOnly = 1 << 2,
			AssetTexture = 1 << 3,
			AssetModel = 1 << 4,
			AssetAudio = 1 << 5,
			AssetPrefab = 1 << 6,
			AssetScene = 1 << 7,
			Multiline = 1 << 8,
		};
		float speed = 0.05f;
		float min = 0.0f;
		float max = 0.0f;
		uint32_t flags = None;
		const char* tooltip = nullptr;
	};
}
