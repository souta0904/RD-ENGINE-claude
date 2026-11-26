#include "RdPch.h"
#include "Components/Audio/AudioSource.h"
#include "Asset/AssetManager.h"
#include "Asset/Audio/AudioClip.h"
#include "Scene/Actor/Actor.h"
#if RD_EDITOR
#include <ImGui/imgui.h>
#endif

namespace rd
{
	void AudioSource::Reflect(PropertyVisitor& v)
	{
		v.Property("Clip", clipPath, { .flags = PropertyMeta::AssetAudio });
		v.Property("Volume", volume, { .speed = 0.01f, .min = 0.0f, .max = 4.0f });
		v.Property("Pitch", pitch, { .speed = 0.01f, .min = 0.1f, .max = 4.0f });
		v.Property("Loop", loop);
		v.Property("Play On Start", playOnStart);
		v.Property("Spatial", spatial);
		v.Property("Min Distance", minDistance, { .speed = 0.1f, .min = 0.01f, .max = 1000.0f });
		v.Property("Max Distance", maxDistance, { .speed = 0.1f, .min = 0.01f, .max = 10000.0f });
		v.Property("Bus", bus);
	}

	void AudioSource::Awake()
	{
		if (!clipPath.empty()) { mClip = AssetManager::Get().LoadAudio(clipPath); }
	}

	void AudioSource::Start()
	{
		if (playOnStart) { Play(); }
	}

	void AudioSource::Play()
	{
		Stop();
		if (clipPath.empty()) { return; }
		if (!mClip || mClip->GetPath() != clipPath) { mClip = AssetManager::Get().LoadAudio(clipPath); }
		if (!mClip->IsReady())
		{
			// 読み込みが終わったら Update で鳴らす
			mPlayRequested = true;
			return;
		}
		PlayParams params;
		params.volume = volume;
		params.pitch = pitch;
		params.loop = loop;
		params.bus = bus;
		params.spatial = spatial;
		params.position = GetTransform().GetPosition();
		params.minDistance = minDistance;
		params.maxDistance = maxDistance;
		mVoice = AudioSystem::Get().Play(mClip, params);
		mAppliedVolume = volume;
		mAppliedPitch = pitch;
		mPlayRequested = false;
	}

	void AudioSource::Stop()
	{
		mPlayRequested = false;
		if (mVoice != AudioSystem::kInvalidVoice)
		{
			AudioSystem::Get().Stop(mVoice);
			mVoice = AudioSystem::kInvalidVoice;
		}
		for (AudioSystem::VoiceId id : mOneShots) { AudioSystem::Get().Stop(id); }
		mOneShots.clear();
		mPendingShots.clear();
	}

	bool AudioSource::IsPlaying() const
	{
		return mVoice != AudioSystem::kInvalidVoice && AudioSystem::Get().IsPlaying(mVoice);
	}

	void AudioSource::PlayOneShot(const std::string& path, float volumeScale)
	{
		if (path.empty()) { return; }
		auto clip = AssetManager::Get().LoadAudio(path);
		if (!clip->IsReady())
		{
			mPendingShots.push_back({ clip, volumeScale });
			return;
		}
		StartOneShot(clip, volumeScale);
	}

	void AudioSource::StartOneShot(const AssetRef<AudioClip>& clip, float volumeScale)
	{
		PlayParams params;
		params.volume = volume * volumeScale;
		params.pitch = pitch;
		params.bus = bus;
		params.spatial = spatial;
		params.position = GetTransform().GetPosition();
		params.minDistance = minDistance;
		params.maxDistance = maxDistance;
		AudioSystem::VoiceId id = AudioSystem::Get().Play(clip, params);
		if (id != AudioSystem::kInvalidVoice) { mOneShots.push_back(id); }
	}

	void AudioSource::Update(float)
	{
		if (mPlayRequested && mClip && mClip->IsReady()) { Play(); }

		// 読み込み待ちの 1 回鳴らす音（読み込みに失敗したものは捨てる）
		if (!mPendingShots.empty())
		{
			std::vector<PendingShot> pending;
			pending.swap(mPendingShots);
			for (PendingShot& shot : pending)
			{
				if (shot.clip->IsReady()) { StartOneShot(shot.clip, shot.volumeScale); }
				else if (!shot.clip->IsFailed()) { mPendingShots.push_back(std::move(shot)); }
			}
		}
		if (!mOneShots.empty())
		{
			std::erase_if(mOneShots, [](AudioSystem::VoiceId id) { return !AudioSystem::Get().IsPlaying(id); });
		}

		if (mVoice != AudioSystem::kInvalidVoice)
		{
			if (volume != mAppliedVolume) { AudioSystem::Get().SetVolume(mVoice, volume); mAppliedVolume = volume; }
			if (pitch != mAppliedPitch) { AudioSystem::Get().SetPitch(mVoice, pitch); mAppliedPitch = pitch; }
		}
		if (spatial && mVoice != AudioSystem::kInvalidVoice)
		{
			AudioSystem::Get().SetPosition(mVoice, GetTransform().GetPosition());
		}
	}

	void AudioSource::OnDestroy()
	{
		Stop();
	}

#if RD_EDITOR
	bool AudioSource::OnInspectorGUI()
	{
		if (ImGui::Button("Preview")) { Play(); }
		ImGui::SameLine();
		if (ImGui::Button("Stop")) { Stop(); }
		return false;
	}
#endif
}
