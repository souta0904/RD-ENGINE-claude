#include "RdPch.h"
#include "Components/AudioSource.h"
#include "Asset/AssetManager.h"
#include "Audio/AudioClip.h"
#include "Scene/Actor.h"
#include <ImGui/imgui.h>

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
		// 先に読み込みを始めておく
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
			// 読み込み完了後に再生する
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
	}

	bool AudioSource::IsPlaying() const
	{
		return mVoice != AudioSystem::kInvalidVoice && AudioSystem::Get().IsPlaying(mVoice);
	}

	void AudioSource::PlayOneShot(const std::string& path, float volumeScale)
	{
		auto clip = AssetManager::Get().LoadAudio(path);
		if (!clip->IsReady()) { return; }
		PlayParams params;
		params.volume = volume * volumeScale;
		params.bus = bus;
		params.spatial = spatial;
		params.position = GetTransform().GetPosition();
		params.minDistance = minDistance;
		params.maxDistance = maxDistance;
		AudioSystem::Get().Play(clip, params);
	}

	void AudioSource::Update(float)
	{
		if (mPlayRequested && mClip && mClip->IsReady()) { Play(); }
		if (spatial && mVoice != AudioSystem::kInvalidVoice)
		{
			AudioSystem::Get().SetPosition(mVoice, GetTransform().GetPosition());
		}
	}

	void AudioSource::OnDestroy()
	{
		Stop();
	}

	void AudioSource::OnInspectorGUI()
	{
		if (ImGui::Button("Preview")) { Play(); }
		ImGui::SameLine();
		if (ImGui::Button("Stop")) { Stop(); }
	}
}
