#include "RdPch.h"
#include "Engine.h"
#include "Asset/AssetManager.h"
#include "Audio/AudioSystem.h"
#include "Components/BuiltinComponents.h"
#include "Components/CameraComponent.h"
#include "Core/JobSystem.h"
#include "Core/Time.h"
#include "Core/Timer.h"
#include "Platform/Input.h"
#include "Render/Renderer.h"
#include "RHI/GraphicsDevice.h"
#include "Scene/Scene.h"
#if RD_EDITOR
#include "Editor/Editor.h"
#endif

namespace rd
{
	Engine::Engine()
	{
		RD_ASSERT(!sInstance);
		sInstance = this;
	}

	Engine::~Engine()
	{
		sInstance = nullptr;
	}

	int Engine::Run(const EngineConfig& config)
	{
		mConfig = config;
		if (!Initialize())
		{
			Shutdown();
			return -1;
		}

		Timer timer;
		while (mRunning)
		{
			Input::NewFrame();
			if (!mWindow.PumpMessages()) { break; }
			Input::LatchFrame();
			if (mWindow.IsMinimized())
			{
				Sleep(10);
				timer.Tick();
				continue;
			}
			float dt = std::min(timer.Tick(), 0.1f);// デバッガ停止などの巨大な dt を防ぐ
			Time::sRealTime = timer.GetTotalSeconds();
			Frame(dt);
		}

		Shutdown();
		return 0;
	}

	void Engine::Quit()
	{
		mRunning = false;
	}

	bool Engine::Initialize()
	{
		CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		Log::Info("{} {}{}", kName, kVersion, RD_EDITOR ? "" : " (Game)");

		JobSystem::Initialize();

		WindowDesc wd = mConfig.window;
		if (wd.title.empty()) { wd.title = kName; }
		if (!mWindow.Create(wd)) { return false; }

		mDevice = std::make_unique<GraphicsDevice>();
		GraphicsDeviceDesc dd;
		dd.hwnd = mWindow.GetHandle();
		dd.width = mWindow.GetWidth();
		dd.height = mWindow.GetHeight();
		dd.vsync = mConfig.vsync;
		if (!mDevice->Initialize(dd)) { return false; }

		Input::Initialize(mWindow.GetHandle());
		mWindow.SetResizeCallback([this](uint32_t w, uint32_t h) { mDevice->Resize(w, h); });

		mAssets = std::make_unique<AssetManager>();
		mAssets->Initialize();
		mRenderer = std::make_unique<Renderer>();
		mRenderer->Initialize();
		mAudio = std::make_unique<AudioSystem>();
		mAudio->Initialize();

		RegisterBuiltinComponents();

		mComposer.Initialize(mWindow, *mDevice, *mRenderer);
		mCapture.Configure(mConfig.captureFrame, mConfig.capturePath);

#if RD_EDITOR
		if (mConfig.enableEditor)
		{
			mEditor = std::make_unique<Editor>();
			mEditor->Initialize();
			mWindow.SetMessageHook([this](HWND h, UINT m, WPARAM w, LPARAM l) { return mEditor->HandleMessage(h, m, w, l); });
			mComposer.SetEditor(mEditor.get());
		}
#endif

		// 初期シーン
		mScenes.CreateInitial(mConfig.startScene, mConfig.buildDefaultScene);

#if RD_EDITOR
		if (mEditor) { mEditor->OnSceneChanged(); }
		mPlay.SetPlaying(!mEditor);
		if (mEditor && mConfig.autoPlay) { BeginPlay(); }
		SetWindowTitleSuffix("");
#else
		mPlay.SetPlaying(true);// エディタなしでは常にプレイ中
#endif
		return true;
	}

	void Engine::Shutdown()
	{
		if (mDevice && GraphicsDevice::IsAlive()) { mDevice->WaitIdle(); }
		// DestroyWindow 中のメッセージが破棄済みの Editor / デバイスへ届かないように外す
		mWindow.SetMessageHook(nullptr);
		mWindow.SetResizeCallback(nullptr);
		mScenes.Clear();
#if RD_EDITOR
		if (mEditor) { mEditor->Shutdown(); mEditor.reset(); }
#endif
		if (mAudio) { mAudio->Shutdown(); mAudio.reset(); }
		if (mRenderer) { mRenderer->Shutdown(); mRenderer.reset(); }
		if (mAssets) { mAssets->Shutdown(); mAssets.reset(); }
		JobSystem::Shutdown();
		if (mDevice) { mDevice->Shutdown(); mDevice.reset(); }
		mWindow.Destroy();
		CoUninitialize();
	}

	void Engine::Frame(float dt)
	{
		mStats.Tick(dt);

		JobSystem::ExecuteMainThreadTasks();
		ApplyPendingScene();

		PlayController::FrameTime time = mPlay.AdvanceTime(dt);

		mDevice->BeginFrame();

#if RD_EDITOR
		// エディタ UI（ビューポートの大きさやカメラもここで決まる。Stop でシーンが差し替わることもある）
		if (mEditor)
		{
			mEditor->BeginFrame();
			mEditor->Update(dt);
		}
		else
#endif
		{
			Input::SetBlocked(false, false);
		}

		// シーン更新（プレイ中でなければ編集用の更新のみ）
		Scene& scene = mScenes.GetScene();
		scene.Update(time.advance ? time.gameDt : dt, time.advance);

		UpdateAudio(scene);

		// 描画 → バックバッファへ
		mCapture.BeforeRender(*mRenderer, mFrameCount);
		mComposer.Render(scene, float(Time::RealTime()));
		mDevice->EndFrame();

		if (mCapture.AfterPresent(*mRenderer, mFrameCount))
		{
			Log::Info("Capture finished, quitting");
			Quit();
		}
		++mFrameCount;
	}

	void Engine::UpdateAudio(Scene& scene)
	{
		// リスナーはゲームカメラ
		if (CameraComponent* mainCam = scene.GetMainCamera())
		{
			mAudio->SetListener(mainCam->GetTransform().GetPosition(), mainCam->GetTransform().GetRight());
		}
		mAudio->Update();
	}

	// ==================================================
	// プレイ（エディタ）
	// ==================================================
#if RD_EDITOR
	void Engine::BeginPlay()
	{
		if (!mPlay.Begin(mScenes.GetScene())) { return; }
		Log::Info("Play");
	}

	void Engine::EndPlay()
	{
		if (!mPlay.IsPlaying() || !mEditor) { return; }
		mAudio->StopAll();
		mScenes.SetScene(mPlay.End(mScenes.GetScene()));
		mEditor->OnSceneChanged(true);
		Log::Info("Stop");
	}
#endif

	// ==================================================
	// シーン
	// ==================================================
	void Engine::ApplyPendingScene()
	{
		if (!mScenes.ApplyPending()) { return; }
#if RD_EDITOR
		if (mEditor)
		{
			mPlay.SetPlaying(false);
			mEditor->OnSceneChanged();
		}
		SetWindowTitleSuffix("");
#endif
	}

#if RD_EDITOR
	void Engine::SetWindowTitleSuffix(const std::string& suffix)
	{
		std::string scene = mScenes.HasScene() ? (mScenes.GetScene().GetPath().empty() ? std::string("Untitled") : mScenes.GetScene().GetPath()) : "";
		mWindow.SetTitle(std::format("{} {} - {}{}", kName, kVersion, scene, suffix));
	}
#endif
}
