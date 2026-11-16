#include "RdPch.h"
#include "Asset/Audio/AudioDecoder.h"
#include "Core/Util/FileUtil.h"
#include "Core/Util/StringUtil.h"
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mmreg.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

namespace rd
{
	using Microsoft::WRL::ComPtr;

	namespace
	{
		bool Fail(const std::string& path, const char* what, HRESULT hr)
		{
			Log::Error(LogCat::Audio, "Failed to decode audio ({} 0x{:08X}): {}", what, uint32_t(hr), path);
			return false;
		}
	}

	bool AudioDecoder::Decode(const std::string& path, std::vector<uint8_t>& format, std::vector<uint8_t>& data)
	{
		format.clear();
		data.clear();
		std::vector<uint8_t> file;
		if (!ReadBinaryFile(path, file))
		{
			Log::Error(LogCat::Audio, "Audio file not found: {}", path);
			return false;
		}
		return IsWav(file) ? DecodeWav(path, file, format, data) : DecodeWithMediaFoundation(path, format, data);
	}

	bool AudioDecoder::IsWav(const std::vector<uint8_t>& file)
	{
		return file.size() >= 12 && std::memcmp(file.data(), "RIFF", 4) == 0 && std::memcmp(file.data() + 8, "WAVE", 4) == 0;
	}

	// RIFF のチャンクを順に見て fmt と data を探す（チャンクは 2 バイト境界にそろっている）
	bool AudioDecoder::DecodeWav(const std::string& path, const std::vector<uint8_t>& file, std::vector<uint8_t>& format, std::vector<uint8_t>& data)
	{
		size_t pos = 12;
		while (pos + 8 <= file.size())
		{
			const char* id = reinterpret_cast<const char*>(file.data() + pos);
			uint32_t size = 0;
			std::memcpy(&size, file.data() + pos + 4, 4);
			size_t body = pos + 8;
			// サイズの欄が壊れたファイル（上位バイトが空白など）への対策
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
			pos = body + size + (size & 1);
		}
		if (format.empty() || data.empty())
		{
			Log::Error(LogCat::Audio, "Invalid WAV file: {}", path);
			return false;
		}
		return true;
	}

	// MFStartup / MFShutdown は参照カウント式なので、呼ぶたびに対にする
	bool AudioDecoder::DecodeWithMediaFoundation(const std::string& path, std::vector<uint8_t>& format, std::vector<uint8_t>& data)
	{
		if (FAILED(MFStartup(MF_VERSION, MFSTARTUP_LITE)))
		{
			Log::Error(LogCat::Audio, "Media Foundation is not available: {}", path);
			return false;
		}
		bool ok = ReadWithMediaFoundation(path, format, data);
		MFShutdown();
		return ok;
	}

	// 最初の音声ストリームだけを 16bit PCM で読む
	bool AudioDecoder::ReadWithMediaFoundation(const std::string& path, std::vector<uint8_t>& format, std::vector<uint8_t>& data)
	{
		ComPtr<IMFSourceReader> reader;
		std::wstring wide = std::filesystem::absolute(ToWide(path)).wstring();
		HRESULT hr = MFCreateSourceReaderFromURL(wide.c_str(), nullptr, &reader);
		if (FAILED(hr)) { return Fail(path, "open", hr); }

		const DWORD stream = DWORD(MF_SOURCE_READER_FIRST_AUDIO_STREAM);
		reader->SetStreamSelection(DWORD(MF_SOURCE_READER_ALL_STREAMS), FALSE);
		reader->SetStreamSelection(stream, TRUE);
		ComPtr<IMFMediaType> request;
		MFCreateMediaType(&request);
		request->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
		request->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
		request->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
		hr = reader->SetCurrentMediaType(stream, nullptr, request.Get());
		if (FAILED(hr)) { return Fail(path, "set PCM output", hr); }

		ComPtr<IMFMediaType> actual;
		hr = reader->GetCurrentMediaType(stream, &actual);
		if (FAILED(hr)) { return Fail(path, "get format", hr); }
		WAVEFORMATEX* wfx = nullptr;
		UINT32 wfxSize = 0;
		hr = MFCreateWaveFormatExFromMFMediaType(actual.Get(), &wfx, &wfxSize);
		if (FAILED(hr)) { return Fail(path, "convert format", hr); }
		format.assign(reinterpret_cast<uint8_t*>(wfx), reinterpret_cast<uint8_t*>(wfx) + wfxSize);
		CoTaskMemFree(wfx);

		while (true)
		{
			DWORD flags = 0;
			ComPtr<IMFSample> sample;
			hr = reader->ReadSample(stream, 0, nullptr, &flags, nullptr, &sample);
			if (FAILED(hr)) { return Fail(path, "read", hr); }
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
		if (data.empty()) { return Fail(path, "no samples", E_FAIL); }
		return true;
	}
}
