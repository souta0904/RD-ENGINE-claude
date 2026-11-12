#pragma once
#include "Asset/Asset.h"
#include "Math/Vector.h"
#include <cstdint>
#include <memory>
#include <vector>

struct IXAudio2;
struct IXAudio2MasteringVoice;
struct IXAudio2SubmixVoice;
struct IXAudio2SourceVoice;

namespace rd
{
	class AudioClip;

	enum class AudioBus : uint8_t { Music, Effects, Count };

	struct PlayParams
	{
		float volume = 1.0f;
		float pitch = 1.0f;
		bool loop = false;
		AudioBus bus = AudioBus::Effects;
		bool spatial = false;// 3D 定位（position を使う）
		Vector3 position;
		float minDistance = 1.0f;// これより近いと音量最大
		float maxDistance = 50.0f;// これより遠いと無音
	};

	// XAudio2 による音声再生
	//  BGM/SE のバスごとの音量、ループ、簡易 3D 定位（距離減衰とパン）に対応
	class AudioSystem
	{
	public:
		using VoiceId = uint32_t;
		static constexpr VoiceId kInvalidVoice = 0;

		static AudioSystem& Get() { return *sInstance; }
		AudioSystem();
		~AudioSystem();

		bool Initialize();
		void Shutdown();
		// 再生終了したボイスの片付けと 3D 定位の更新
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
		struct Voice;
		void ApplySpatial(Voice& voice);
		Voice* Find(VoiceId id);
		const Voice* Find(VoiceId id) const;

		static inline AudioSystem* sInstance = nullptr;
		IXAudio2* mXAudio = nullptr;
		IXAudio2MasteringVoice* mMaster = nullptr;
		IXAudio2SubmixVoice* mBuses[size_t(AudioBus::Count)] = {};
		float mBusVolumes[size_t(AudioBus::Count)] = { 1.0f, 1.0f };
		uint32_t mOutputChannels = 2;
		std::vector<std::unique_ptr<Voice>> mVoices;
		VoiceId mNextId = 1;
		Vector3 mListenerPos;
		Vector3 mListenerRight = Vector3::kUnitX;
	};
}
