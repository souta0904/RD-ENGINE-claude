#pragma once
#include "Audio/AudioVoice.h"
#include <memory>
#include <vector>

struct IXAudio2;
struct IXAudio2MasteringVoice;
struct IXAudio2SubmixVoice;

namespace rd
{
	// XAudio2 での音の再生（BGM / 効果音のバスごとの音量、ループ、簡単な 3D（距離での減衰と左右のパン））
	class AudioSystem
	{
	public:
		using VoiceId = uint32_t;
		static constexpr VoiceId kInvalidVoice = 0;

		static AudioSystem& Get() { return *sInstance; }

		bool Initialize();
		void Shutdown();
		// 鳴り終わった音を片付け、3D の音の位置を反映する
		void Update();

		VoiceId Play(const AssetRef<AudioClip>& clip, const PlayParams& params = {});
		void Stop(VoiceId id);
		void StopAll();
		void Pause(VoiceId id, bool pause);
		bool IsPlaying(VoiceId id) const;
		void SetVolume(VoiceId id, float volume);
		void SetPitch(VoiceId id, float pitch);
		void SetPosition(VoiceId id, const Vector3& position);

		void SetMasterVolume(float volume);
		void SetBusVolume(AudioBus bus, float volume);
		float GetBusVolume(AudioBus bus) const { return mBusVolumes[size_t(bus)]; }
		void SetListener(const Vector3& position, const Vector3& right);
		uint32_t GetActiveVoiceCount() const { return uint32_t(mVoices.size()); }

	private:
		void ApplySpatial(AudioVoice& voice);
		AudioVoice* Find(VoiceId id);
		const AudioVoice* Find(VoiceId id) const;

		static inline AudioSystem* sInstance = nullptr;
		IXAudio2* mXAudio = nullptr;
		IXAudio2MasteringVoice* mMaster = nullptr;
		IXAudio2SubmixVoice* mBuses[size_t(AudioBus::Count)] = {};
		float mBusVolumes[size_t(AudioBus::Count)] = { 1.0f, 1.0f };
		uint32_t mOutputChannels = 2;
		std::vector<std::unique_ptr<AudioVoice>> mVoices;
		VoiceId mNextId = 1;
		Vector3 mListenerPos;
		Vector3 mListenerRight = Vector3::kUnitX;
	};
}
