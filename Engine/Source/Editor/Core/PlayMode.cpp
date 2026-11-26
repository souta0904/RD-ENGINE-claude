#include "RdPch.h"
#include "Editor/Core/PlayMode.h"
#include "Audio/AudioSystem.h"
#include "Editor/Editor.h"
#include "Engine.h"
#include "Scene/Scene.h"
#include "Scene/Serialization/SceneSerializer.h"

namespace rd
{
	bool PlayMode::IsPlaying() const
	{
		return mEditor.GetEngine().IsPlaying();
	}

	bool PlayMode::IsPaused() const
	{
		return mEditor.GetEngine().IsPaused();
	}

	void PlayMode::Begin()
	{
		if (IsPlaying()) { return; }
		const Scene& scene = mEditor.GetScene();
		mSnapshot = SceneSerializer::Serialize(scene).dump();
		mSnapshotPath = scene.GetPath();
		mEditor.GetEngine().GetPlayController().Start();
		Logger::Info(LogCat::Core, "Play");
	}

	// プレイ中にゲームが別の場面へ切り替えていても、プレイを始めた場面（パスも）に戻す
	void PlayMode::End()
	{
		if (!IsPlaying()) { return; }
		Engine& engine = mEditor.GetEngine();
		AudioSystem::Get().StopAll();
		engine.GetPlayController().Stop();
		auto scene = std::make_unique<Scene>();
		SceneSerializer::Deserialize(*scene, Json::parse(mSnapshot));
		scene->SetPath(mSnapshotPath);
		engine.GetSceneManager().SetScene(std::move(scene));
		mEditor.SceneChanged(true);
		Logger::Info(LogCat::Core, "Stop");
	}

	void PlayMode::Toggle()
	{
		IsPlaying() ? End() : Begin();
	}

	void PlayMode::TogglePause()
	{
		if (IsPlaying()) { mEditor.GetEngine().SetPaused(!IsPaused()); }
	}

	void PlayMode::Step()
	{
		if (!IsPlaying()) { return; }
		mEditor.GetEngine().SetPaused(true);
		mEditor.GetEngine().StepFrame();
	}
}
