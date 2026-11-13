#include "RdPch.h"
#include "Asset/AudioClip.h"
#include "Core/StringUtil.h"
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

namespace rd
{
	using Microsoft::WRL::ComPtr;

	namespace
	{
		bool IsWav(const std::vector<uint8_t>& file)
		{
			return file.size() >= 12 && std::memcmp(file.data(), "RIFF", 4) == 0 && std::memcmp(file.data() + 8, "WAVE", 4) == 0;
		}

		bool DecodeWav(const std::string& path, const std::vector<uint8_t>& file, std::vector<uint8_t>& format, std::vector<uint8_t>& data)
		{
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
					format.assign(file.begin() + body, file.begin() + body + size);
					if (format.size() < sizeof(WAVEFORMATEX)) { format.resize(sizeof(WAVEFORMATEX), 0); }
				}
				else if (std::memcmp(id, "data", 4) == 0)
				{
					data.assign(file.begin() + body, file.begin() + body + size);
				}
				pos = body + size + (size & 1);// チャンクは 2 バイト境界
			}
			if (format.empty() || data.empty())
			{
				Log::Error(LogCat::Audio, "Invalid WAV file: {}", path);
				return false;
			}
			return true;
		}

		// MP3 / AAC(m4a) / FLAC / WMA を Windows の Media Foundation で 16bit PCM に展開する
		//  （Ogg Vorbis は Windows 標準では読めない）
		bool DecodeWithMediaFoundation(const std::string& path, std::vector<uint8_t>& format, std::vector<uint8_t>& data)
		{
			// COM は呼ぶスレッドで初期化済みであること（JobSystem のワーカーとメインスレッドは初期化済み）
			if (FAILED(MFStartup(MF_VERSION, MFSTARTUP_LITE)))
			{
				Log::Error(LogCat::Audio, "Media Foundation is not available: {}", path);
				return false;
			}
			struct Shutdown { ~Shutdown() { MFShutdown(); } } shutdown;// MFStartup / MFShutdown は参照カウント式

			auto fail = [&](const char* what, HRESULT hr)
				{
					Log::Error(LogCat::Audio, "Failed to decode audio ({} 0x{:08X}): {}", what, uint32_t(hr), path);
					return false;
				};

			ComPtr<IMFSourceReader> reader;
			std::wstring wide = std::filesystem::absolute(ToWide(path)).wstring();
			HRESULT hr = MFCreateSourceReaderFromURL(wide.c_str(), nullptr, &reader);
			if (FAILED(hr)) { return fail("open", hr); }

			// 最初の音声ストリームだけを 16bit PCM で読む
			const DWORD stream = DWORD(MF_SOURCE_READER_FIRST_AUDIO_STREAM);
			reader->SetStreamSelection(DWORD(MF_SOURCE_READER_ALL_STREAMS), FALSE);
			reader->SetStreamSelection(stream, TRUE);
			ComPtr<IMFMediaType> request;
			MFCreateMediaType(&request);
			request->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
			request->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
			request->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
			hr = reader->SetCurrentMediaType(stream, nullptr, request.Get());
			if (FAILED(hr)) { return fail("set PCM output", hr); }

			ComPtr<IMFMediaType> actual;
			hr = reader->GetCurrentMediaType(stream, &actual);
			if (FAILED(hr)) { return fail("get format", hr); }
			WAVEFORMATEX* wfx = nullptr;
			UINT32 wfxSize = 0;
			hr = MFCreateWaveFormatExFromMFMediaType(actual.Get(), &wfx, &wfxSize);
			if (FAILED(hr)) { return fail("convert format", hr); }
			format.assign(reinterpret_cast<uint8_t*>(wfx), reinterpret_cast<uint8_t*>(wfx) + wfxSize);
			CoTaskMemFree(wfx);

			while (true)
			{
				DWORD flags = 0;
				ComPtr<IMFSample> sample;
				hr = reader->ReadSample(stream, 0, nullptr, &flags, nullptr, &sample);
				if (FAILED(hr)) { return fail("read", hr); }
				if (flags & MF_SOURCE_READERF_ENDOFSTREAM) { break; }
				if (!sample) { continue; }
				ComPtr<IMFMediaBuffer> buffer;
				if (FAILED(sample->ConvertToContiguousBuffer(&buffer))) { continue; }
				BYTE* bytes = nullptr;
				DWORD length = 0;
				if (SUCCEEDED(buffer->Lock(&bytes, nullptr, &length)))
				{
					data.insert(data.end(), bytes, bytes + length);
					buffer->Unlock();
				}
			}
			if (data.empty()) { return fail("no samples", E_FAIL); }
			return true;
		}
	}

	bool AudioClip::IsSupportedExtension(const std::string& ext)
	{
		return ext == ".wav" || ext == ".mp3" || ext == ".m4a" || ext == ".aac" || ext == ".flac" || ext == ".wma";
	}

	bool AudioClip::Decode(const std::string& path, std::vector<uint8_t>& format, std::vector<uint8_t>& data)
	{
		format.clear();
		data.clear();
		std::vector<uint8_t> file;
		if (!ReadBinaryFile(path, file))
		{
			Log::Error(LogCat::Audio, "Audio file not found: {}", path);
			return false;
		}
		// WAV は自前で読む（ADPCM などもそのまま XAudio2 に渡せる）。それ以外は Media Foundation で PCM に展開する
		return IsWav(file) ? DecodeWav(path, file, format, data) : DecodeWithMediaFoundation(path, format, data);
	}

	void AudioClip::Load()
	{
		if (!Decode(mPath, mFormat, mData))
		{
			SetState(AssetState::Failed);
			return;
		}
		const WAVEFORMATEX* fmt = GetFormat();
		mDuration = fmt->nAvgBytesPerSec > 0 ? float(mData.size()) / float(fmt->nAvgBytesPerSec) : 0.0f;
		SetState(AssetState::Ready);
	}
}
