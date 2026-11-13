#include "RdPch.h"
#include "Asset/AudioClip.h"
#include "Core/StringUtil.h"

namespace rd
{
	void AudioClip::Load()
	{
		std::vector<uint8_t> file;
		if (!ReadBinaryFile(mPath, file) || file.size() < 12 ||
			std::memcmp(file.data(), "RIFF", 4) != 0 || std::memcmp(file.data() + 8, "WAVE", 4) != 0)
		{
			Log::Error(LogCat::Audio, "Failed to load audio (WAV only): {}", mPath);
			SetState(AssetState::Failed);
			return;
		}

		// チャンクを走査して fmt と data を探す
		size_t pos = 12;
		while (pos + 8 <= file.size())
		{
			const char* id = reinterpret_cast<const char*>(file.data() + pos);
			uint32_t size = 0;
			std::memcpy(&size, file.data() + pos + 4, 4);
			size_t body = pos + 8;
			// サイズ欄が壊れたファイル（上位バイトが空白など）への対策
			if (std::memcmp(id, "fmt ", 4) == 0 && size > 256) { size = 16; }
			if (body + size > file.size()) { size = uint32_t(file.size() - body); }

			if (std::memcmp(id, "fmt ", 4) == 0)
			{
				mFormat.assign(file.begin() + body, file.begin() + body + size);
				if (mFormat.size() < sizeof(WAVEFORMATEX)) { mFormat.resize(sizeof(WAVEFORMATEX), 0); }
			}
			else if (std::memcmp(id, "data", 4) == 0)
			{
				mData.assign(file.begin() + body, file.begin() + body + size);
			}
			pos = body + size + (size & 1);// チャンクは 2 バイト境界
		}

		if (mFormat.empty() || mData.empty())
		{
			Log::Error(LogCat::Audio, "Invalid WAV file: {}", mPath);
			SetState(AssetState::Failed);
			return;
		}
		const WAVEFORMATEX* fmt = GetFormat();
		mDuration = fmt->nAvgBytesPerSec > 0 ? float(mData.size()) / float(fmt->nAvgBytesPerSec) : 0.0f;
		SetState(AssetState::Ready);
	}
}
