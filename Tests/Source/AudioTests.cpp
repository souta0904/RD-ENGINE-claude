#include "TestFramework.h"
#include "Asset/AudioClip.h"
#include "Core/StringUtil.h"
#include <Windows.h>
#include <wrl/client.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <cmath>
#include <filesystem>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

using Microsoft::WRL::ComPtr;

namespace
{
	// テスト用の音声ファイルを Windows 標準の AAC エンコーダで作る（1 秒・440Hz・モノラル）
	bool WriteSineM4a(const std::wstring& path)
	{
		constexpr UINT32 kRate = 44100;
		ComPtr<IMFSinkWriter> writer;
		if (FAILED(MFCreateSinkWriterFromURL(path.c_str(), nullptr, nullptr, &writer))) { return false; }

		ComPtr<IMFMediaType> outType, inType;
		MFCreateMediaType(&outType);
		outType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
		outType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC);
		outType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
		outType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, kRate);
		outType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 1);
		outType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 16000);
		DWORD stream = 0;
		if (FAILED(writer->AddStream(outType.Get(), &stream))) { return false; }

		MFCreateMediaType(&inType);
		inType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
		inType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
		inType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
		inType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, kRate);
		inType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 1);
		inType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 2);
		inType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, kRate * 2);
		if (FAILED(writer->SetInputMediaType(stream, inType.Get(), nullptr))) { return false; }
		if (FAILED(writer->BeginWriting())) { return false; }

		ComPtr<IMFMediaBuffer> buffer;
		MFCreateMemoryBuffer(kRate * 2, &buffer);
		BYTE* bytes = nullptr;
		buffer->Lock(&bytes, nullptr, nullptr);
		auto samples = reinterpret_cast<int16_t*>(bytes);
		for (UINT32 i = 0; i < kRate; ++i) { samples[i] = int16_t(std::sin(float(i) * 440.0f * 6.2831853f / float(kRate)) * 12000.0f); }
		buffer->Unlock();
		buffer->SetCurrentLength(kRate * 2);
		ComPtr<IMFSample> sample;
		MFCreateSample(&sample);
		sample->AddBuffer(buffer.Get());
		sample->SetSampleTime(0);
		sample->SetSampleDuration(10'000'000);
		if (FAILED(writer->WriteSample(stream, sample.Get()))) { return false; }
		return SUCCEEDED(writer->Finalize());
	}

	std::string FindAssets()
	{
		wchar_t exe[MAX_PATH];
		GetModuleFileNameW(nullptr, exe, MAX_PATH);
		for (auto dir = std::filesystem::path(exe).parent_path(); dir.has_parent_path() && dir != dir.parent_path(); dir = dir.parent_path())
		{
			if (std::filesystem::exists(dir / "Assets")) { return rd::ToUtf8((dir / "Assets").wstring()); }
		}
		return "Assets";
	}
}

RD_TEST(Audio, DecodeWav)
{
	std::vector<uint8_t> format, data;
	RD_REQUIRE(rd::AudioClip::Decode(FindAssets() + "/Sample/Audio/ChipGet.wav", format, data));
	RD_CHECK(format.size() >= sizeof(WAVEFORMATEX));
	RD_CHECK(!data.empty());
}

RD_TEST(Audio, DecodeAacWithMediaFoundation)
{
	CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	RD_REQUIRE(SUCCEEDED(MFStartup(MF_VERSION, MFSTARTUP_LITE)));
	std::filesystem::path path = std::filesystem::temp_directory_path() / L"RdEngineAudioTest.m4a";
	bool written = WriteSineM4a(path.wstring());
	MFShutdown();
	RD_REQUIRE(written);

	std::vector<uint8_t> format, data;
	RD_REQUIRE(rd::AudioClip::Decode(rd::ToUtf8(path.wstring()), format, data));
	auto fmt = reinterpret_cast<const WAVEFORMATEX*>(format.data());
	RD_CHECK_EQ(fmt->wFormatTag, WORD(WAVE_FORMAT_PCM));
	RD_CHECK_EQ(fmt->wBitsPerSample, WORD(16));
	RD_CHECK_EQ(fmt->nSamplesPerSec, DWORD(44100));
	float seconds = float(data.size()) / float(fmt->nAvgBytesPerSec);
	RD_CHECK(seconds > 0.9f && seconds < 1.2f);
	// 音が入っている（無音ではない）
	auto samples = reinterpret_cast<const int16_t*>(data.data());
	double sum = 0.0;
	size_t count = data.size() / 2;
	for (size_t i = 0; i < count; ++i) { sum += double(samples[i]) * samples[i]; }
	RD_CHECK(std::sqrt(sum / double(count)) > 3000.0);
	std::filesystem::remove(path);
	CoUninitialize();
}

RD_TEST(Audio, UnsupportedFileFails)
{
	std::vector<uint8_t> format, data;
	RD_CHECK(!rd::AudioClip::Decode("does/not/exist.mp3", format, data));
	RD_CHECK(rd::AudioClip::IsSupportedExtension(".mp3"));
	RD_CHECK(!rd::AudioClip::IsSupportedExtension(".ogg"));
}
