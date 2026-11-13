#pragma once
#include "Asset/Asset.h"
#include "Audio/AudioSystem.h"
#include "Scene/Component.h"

namespace rd
{
	class AudioClip;

	// 音を鳴らすコンポーネント（3D 定位はアクターの位置を使う）
	class AudioSource final : public Component
	{
		RD_COMPONENT(AudioSource)
	public:
		std::string clipPath;
		float volume = 1.0f;
		float pitch = 1.0f;
		bool loop = false;
		bool playOnStart = false;
		bool spatial = false;
		float minDistance = 2.0f;
		float maxDistance = 50.0f;
		AudioBus bus = AudioBus::Effects;

		void Reflect(PropertyVisitor& v) override;
		void Awake() override;
		void Start() override;
		void Update(float dt) override;
		void OnDestroy() override;
#if RD_EDITOR
		void OnInspectorGUI() override;
#endif

		void Play();
		void Stop();
		bool IsPlaying() const;
		// 一回だけ鳴らす（重ねて鳴らせる）
		void PlayOneShot(const std::string& path, float volumeScale = 1.0f);

	private:
		AssetRef<AudioClip> mClip;
		AudioSystem::VoiceId mVoice = AudioSystem::kInvalidVoice;
		bool mPlayRequested = false;
	};
}
