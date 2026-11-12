#pragma once
#include "Asset/Asset.h"
#include <Windows.h>
#include <mmreg.h>
#include <vector>

namespace rd
{
	// 音声データ（WAV: PCM / IEEE float / ADPCM）
	class AudioClip final : public Asset
	{
	public:
		const char* GetTypeName() const override { return "AudioClip"; }

		const WAVEFORMATEX* GetFormat() const { return reinterpret_cast<const WAVEFORMATEX*>(mFormat.data()); }
		const std::vector<uint8_t>& GetData() const { return mData; }
		float GetDuration() const { return mDuration; }

	protected:
		void Load() override;

	private:
		std::vector<uint8_t> mFormat;// WAVEFORMATEX（拡張部分を含む）
		std::vector<uint8_t> mData;
		float mDuration = 0.0f;
	};
}
