#include "RdPch.h"
#include "SceneManager.h"
#include "Components/CameraComponent.h"
#include "Components/Lights.h"
#include "Core/StringUtil.h"
#include "Scene/Scene.h"

namespace rd
{
	SceneManager::SceneManager() = default;
	SceneManager::~SceneManager() = default;

	void SceneManager::CreateInitial(const std::string& path, const std::function<void(Scene&)>& build)
	{
		mScene = std::make_unique<Scene>();
		if (!path.empty() && std::filesystem::exists(ToWide(path)))
		{
			mScene->LoadFromFile(path);
		}
		else if (build)
		{
			build(*mScene);
			if (!path.empty()) { mScene->SetPath(path); }
		}
	}

	void SceneManager::LoadScene(const std::string& path)
	{
		mPendingScenePath = path;
	}

	void SceneManager::NewScene()
	{
		// 空のシーン（太陽とカメラだけ）
		auto scene = std::make_unique<Scene>();
		Actor* sun = scene->CreateActor("Directional Light");
		sun->AddComponent<DirectionalLight>();
		sun->GetTransform().SetLocalRotation(Quaternion::FromEuler({ math::ToRadians(50.0f), math::ToRadians(-30.0f), 0.0f }));
		Actor* cam = scene->CreateActor("Main Camera");
		cam->AddComponent<CameraComponent>();
		cam->GetTransform().SetLocalPosition({ 0.0f, 3.0f, -10.0f });
		ReplaceScene(std::move(scene));
	}

	void SceneManager::ReplaceScene(std::unique_ptr<Scene> scene)
	{
		mPendingScene = std::move(scene);
	}

	bool SceneManager::ApplyPending()
	{
		if (!mPendingScenePath.empty())
		{
			auto scene = std::make_unique<Scene>();
			if (scene->LoadFromFile(mPendingScenePath)) { mPendingScene = std::move(scene); }
			mPendingScenePath.clear();
		}
		if (!mPendingScene) { return false; }
		if (mBeforeSwap) { mBeforeSwap(); }
		mScene = std::move(mPendingScene);
		return true;
	}

	void SceneManager::SetScene(std::unique_ptr<Scene> scene)
	{
		if (mBeforeSwap) { mBeforeSwap(); }
		mScene = std::move(scene);
	}

	void SceneManager::Clear()
	{
		mScene.reset();
		mPendingScene.reset();
	}
}
