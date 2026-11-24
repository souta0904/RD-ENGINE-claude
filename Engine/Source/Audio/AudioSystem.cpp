#include "RdPch.h"
#include "Audio/AudioSystem.h"
#include "Asset/Audio/AudioClip.h"
#include <xaudio2.h>

#pragma comment(lib, "xaudio2.lib")

namespace rd
{
	bool AudioSystem::Initialize()
	{
		RD_ASSERT(!sInstance);
		sInstance = this;
		if (FAILED(XAudio2Create(&mXAudio, 0, XAUDIO2_DEFAULT_PROCESSOR)))
		{
			Logger::Error(LogCat::Audio, "XAudio2Create failed (audio disabled)");
			return false;
		}
		if (FAILED(mXAudio->CreateMasteringVoice(&mMaster)))
		{
			Logger::Error(LogCat::Audio, "No audio device (audio disabled)");
			mXAudio->Release();
			mXAudio = nullptr;
			return false;
		}
		XAUDIO2_VOICE_DETAILS details = {};
		mMaster->GetVoiceDetails(&details);
		mOutputChannels = details.InputChannels;
		for (auto& bus : mBuses)
		{
			mXAudio->CreateSubmixVoice(&bus, mOutputChannels, details.InputSampleRate);
		}
		return true;
	}

	void AudioSystem::Shutdown()
	{
		StopAll();
		for (auto& bus : mBuses)
		{
			if (bus) { bus->DestroyVoice(); bus = nullptr; }
		}
		if (mMaster) { mMaster->DestroyVoice(); mMaster = nullptr; }
		if (mXAudio) { mXAudio->Release(); mXAudio = nullptr; }
		sInstance = nullptr;
	}

	void AudioSystem::Update()
	{
		for (auto it = mVoices.begin(); it != mVoices.end();)
		{
			AudioVoice& v = **it;
			XAUDIO2_VOICE_STATE state = {};
			v.source->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
			if (state.BuffersQueued == 0)
			{
				v.source->DestroyVoice();
				it = mVoices.erase(it);
				continue;
			}
			if (v.params.spatial) { ApplySpatial(v); }
			++it;
		}
	}

	AudioSystem::VoiceId AudioSystem::Play(const AssetRef<AudioClip>& clip, const PlayParams& params)
	{
		if (!mXAudio || !clip || !clip->IsReady()) { return kInvalidVoice; }

		auto voice = std::make_unique<AudioVoice>();
		voice->id = mNextId++;
		voice->clip = clip;
		voice->params = params;
		voice->channels = clip->GetFormat()->nChannels;

		XAUDIO2_SEND_DESCRIPTOR send = { 0, mBuses[size_t(params.bus)] };
		XAUDIO2_VOICE_SENDS sends = { 1, &send };
		if (FAILED(mXAudio->CreateSourceVoice(&voice->source, clip->GetFormat(), 0, XAUDIO2_MAX_FREQ_RATIO, nullptr, &sends)))
		{
			Logger::Error(LogCat::Audio, "Failed to create source voice for {}", clip->GetPath());
			return kInvalidVoice;
		}

		XAUDIO2_BUFFER buffer = {};
		buffer.pAudioData = clip->GetData().data();
		buffer.AudioBytes = UINT32(clip->GetData().size());
		buffer.Flags = XAUDIO2_END_OF_STREAM;
		buffer.LoopCount = params.loop ? XAUDIO2_LOOP_INFINITE : 0;
		voice->source->SubmitSourceBuffer(&buffer);
		voice->source->SetVolume(params.volume);
		voice->source->SetFrequencyRatio(params.pitch);
		if (params.spatial) { ApplySpatial(*voice); }
		voice->source->Start();

		VoiceId id = voice->id;
		mVoices.push_back(std::move(voice));
		return id;
	}

	void AudioSystem::Stop(VoiceId id)
	{
		for (auto it = mVoices.begin(); it != mVoices.end(); ++it)
		{
			if ((*it)->id == id)
			{
				(*it)->source->Stop();
				(*it)->source->DestroyVoice();
				mVoices.erase(it);
				return;
			}
		}
	}

	void AudioSystem::StopAll()
	{
		for (auto& v : mVoices)
		{
			v->source->Stop();
			v->source->DestroyVoice();
		}
		mVoices.clear();
	}

	void AudioSystem::Pause(VoiceId id, bool pause)
	{
		if (auto v = Find(id)) { pause ? v->source->Stop() : v->source->Start(); }
	}

	bool AudioSystem::IsPlaying(VoiceId id) const { return Find(id) != nullptr; }

	void AudioSystem::SetVolume(VoiceId id, float volume)
	{
		if (auto v = Find(id))
		{
			v->params.volume = volume;
			if (!v->params.spatial) { v->source->SetVolume(volume); }
		}
	}

	void AudioSystem::SetPitch(VoiceId id, float pitch)
	{
		if (auto v = Find(id)) { v->params.pitch = pitch; v->source->SetFrequencyRatio(pitch); }
	}

	void AudioSystem::SetPosition(VoiceId id, const Vector3& position)
	{
		if (auto v = Find(id)) { v->params.position = position; }
	}

	void AudioSystem::SetMasterVolume(float volume)
	{
		if (mMaster) { mMaster->SetVolume(volume); }
	}

	void AudioSystem::SetBusVolume(AudioBus bus, float volume)
	{
		mBusVolumes[size_t(bus)] = volume;
		if (mBuses[size_t(bus)]) { mBuses[size_t(bus)]->SetVolume(volume); }
	}

	void AudioSystem::SetListener(const Vector3& position, const Vector3& right)
	{
		mListenerPos = position;
		mListenerRight = Normalize(right);
	}

	void AudioSystem::ApplySpatial(AudioVoice& v)
	{
		// 距離の逆数で弱め、maxDistance に向けて 0 まで下げる
		Vector3 toSource = v.params.position - mListenerPos;
		float dist = Length(toSource);
		float att = 1.0f;
		if (dist > v.params.minDistance)
		{
			att = v.params.minDistance / dist;
			float fade = math::Saturate((v.params.maxDistance - dist) / std::max(v.params.maxDistance * 0.2f, 0.001f));
			att *= fade;
		}
		v.source->SetVolume(v.params.volume * att);

		// 左右のパン（ステレオ以上の出力のとき。pan は -1 が左、1 が右）
		if (mOutputChannels < 2) { return; }
		float pan = dist > 1.0e-3f ? Dot(toSource / dist, mListenerRight) : 0.0f;
		float left = std::sqrt(0.5f * (1.0f - pan));
		float right = std::sqrt(0.5f * (1.0f + pan));
		std::vector<float> matrix(size_t(v.channels) * mOutputChannels, 0.0f);
		for (uint32_t c = 0; c < v.channels; ++c)
		{
			// 出力行列の並びは [出力チャンネル][入力チャンネル]
			matrix[0 * v.channels + c] = left;
			matrix[1 * v.channels + c] = right;
		}
		v.source->SetOutputMatrix(mBuses[size_t(v.params.bus)], v.channels, mOutputChannels, matrix.data());
	}

	AudioVoice* AudioSystem::Find(VoiceId id)
	{
		for (auto& v : mVoices) { if (v->id == id) { return v.get(); } }
		return nullptr;
	}

	const AudioVoice* AudioSystem::Find(VoiceId id) const
	{
		for (auto& v : mVoices) { if (v->id == id) { return v.get(); } }
		return nullptr;
	}
}
