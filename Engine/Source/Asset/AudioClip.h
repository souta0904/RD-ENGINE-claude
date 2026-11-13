#pragma once
#include "Asset/Asset.h"
#include <Windows.h>
#include <mmreg.h>
#include <string>
#include <vector>

namespace rd
{
	// 音声データ
	//  WAV（PCM / IEEE float / ADPCM）はそのまま、MP3 / AAC(m4a) / FLAC / WMA は読み込み時に PCM へ展開する
	class AudioClip final : public Asset
	{
	public:
		const char* GetTypeName() const override { return "AudioClip"; }
		// 読み込める拡張子か（".wav" など小文字で）
		static bool IsSupportedExtension(const std::string& ext);
		// ファイルを WAVEFORMATEX と波形データに展開する（アセット管理を通さずに使える。COM を初期化したスレッドで呼ぶ）
		static bool Decode(const std::string& path, std::vector<uint8_t>& format, std::vector<uint8_t>& data);

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
