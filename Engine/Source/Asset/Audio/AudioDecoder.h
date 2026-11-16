#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace rd
{
	// 音声ファイルを WAVEFORMATEX と波形データに展開する
	//  WAV は自分で読む（ADPCM などもそのまま XAudio2 に渡せる）。それ以外は Media Foundation で 16bit PCM にする
	//  （Ogg Vorbis は Windows の標準では読めない）。COM を初期化したスレッドで呼ぶこと
	class AudioDecoder
	{
	public:
		static bool Decode(const std::string& path, std::vector<uint8_t>& format, std::vector<uint8_t>& data);

	private:
		static bool IsWav(const std::vector<uint8_t>& file);
		static bool DecodeWav(const std::string& path, const std::vector<uint8_t>& file, std::vector<uint8_t>& format, std::vector<uint8_t>& data);
		static bool DecodeWithMediaFoundation(const std::string& path, std::vector<uint8_t>& format, std::vector<uint8_t>& data);
		static bool ReadWithMediaFoundation(const std::string& path, std::vector<uint8_t>& format, std::vector<uint8_t>& data);
	};
}
