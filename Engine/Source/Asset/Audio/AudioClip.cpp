#include "RdPch.h"
#include "Asset/Audio/AudioClip.h"
#include "Asset/Audio/AudioDecoder.h"

namespace rd
{
	bool AudioClip::IsSupportedExtension(const std::string& ext)
	{
		return ext == ".wav" || ext == ".mp3" || ext == ".m4a" || ext == ".aac" || ext == ".flac" || ext == ".wma";
	}

	void AudioClip::Load()
	{
		if (!AudioDecoder::Decode(mPath, mFormat, mData))
		{
			SetState(AssetState::Failed);
			return;
		}
		const WAVEFORMATEX* fmt = GetFormat();
		mDuration = fmt->nAvgBytesPerSec > 0 ? float(mData.size()) / float(fmt->nAvgBytesPerSec) : 0.0f;
		SetState(AssetState::Ready);
	}
}
