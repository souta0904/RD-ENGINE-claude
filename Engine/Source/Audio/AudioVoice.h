#pragma once
#include "Asset/Asset.h"
#include "Audio/PlayParams.h"

struct IXAudio2SourceVoice;

namespace rd
{
	class AudioClip;

	// 再生中の音 1 つ（再生している間は clip を持ってデータを消さない）
	struct AudioVoice
	{
		uint32_t id = 0;
		IXAudio2SourceVoice* source = nullptr;
		AssetRef<AudioClip> clip;
		PlayParams params;
		uint32_t channels = 1;
	};
}
