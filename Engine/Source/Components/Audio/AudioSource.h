#pragma once
#include "Asset/Asset.h"
#include "Audio/AudioSystem.h"
#include "Scene/Component/Component.h"
#include <vector>

namespace rd
{
	class AudioClip;

	// 音を鳴らすコンポーネント（3D の音はアクターの位置から鳴る）
	class RD_API AudioSource final : public Component
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

		void Play();
		void Stop();
		bool IsPlaying() const;
		// 1 回だけ鳴らす（重ねて鳴らせる。今の Pitch で鳴らし、Stop で一緒に止まる）
		//  読み込みが終わっていなければ、終わったときに鳴らす
		void PlayOneShot(const std::string& path, float volumeScale = 1.0f);

	private:
		struct PendingShot
		{
			AssetRef<AudioClip> clip;
			float volumeScale = 1.0f;
		};
		void StartOneShot(const AssetRef<AudioClip>& clip, float volumeScale);

		AssetRef<AudioClip> mClip;
		AudioSystem::VoiceId mVoice = AudioSystem::kInvalidVoice;
		bool mPlayRequested = false;
		// 鳴っている間に Volume / Pitch を変えたら反映する
		float mAppliedVolume = 1.0f;
		float mAppliedPitch = 1.0f;
		std::vector<PendingShot> mPendingShots;
		std::vector<AudioSystem::VoiceId> mOneShots;
	};
}
