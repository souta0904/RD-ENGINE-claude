#pragma once
#include "Math/Vector3.h"
#include <cstdint>

namespace rd
{
	enum class AudioBus : uint8_t { Music, Effects, Count };

	// 再生の設定（spatial なら position を使って 3D に置く。minDistance より近いと最大音量、maxDistance より遠いと無音）
	struct PlayParams
	{
		float volume = 1.0f;
		float pitch = 1.0f;
		bool loop = false;
		AudioBus bus = AudioBus::Effects;
		bool spatial = false;
		Vector3 position;
		float minDistance = 1.0f;
		float maxDistance = 50.0f;
	};
}
