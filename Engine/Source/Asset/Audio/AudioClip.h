#pragma once
#include "Asset/Asset.h"
#include <Windows.h>
#include <mmreg.h>
#include <string>
#include <vector>

namespace rd
{
	// 音声データ
	//  WAV（PCM / IEEE float / ADPCM）はそのまま、MP3 / AAC(m4a) / FLAC / WMA は読み込むときに PCM へ展開する
	//  mFormat は拡張部分を含む WAVEFORMATEX
	class RD_API AudioClip final : public Asset
	{
	public:
		const char* GetTypeName() const override { return "AudioClip"; }
		// ext は ".wav" などの小文字
		static bool IsSupportedExtension(const std::string& ext);

		const WAVEFORMATEX* GetFormat() const { return reinterpret_cast<const WAVEFORMATEX*>(mFormat.data()); }
		const std::vector<uint8_t>& GetData() const { return mData; }
		float GetDuration() const { return mDuration; }

	protected:
		void Load() override;

	private:
		std::vector<uint8_t> mFormat;
		std::vector<uint8_t> mData;
		float mDuration = 0.0f;
	};
}
