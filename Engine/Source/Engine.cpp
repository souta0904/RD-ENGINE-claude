#include "RdPch.h"
#include "Engine.h"
#include "Asset/AssetManager.h"
#include "Audio/AudioSystem.h"
#include "Runtime/BuiltinComponents.h"
#include "Components/CameraComponent.h"
#include "Core/CrashHandler.h"
#include "Core/JobSystem.h"
#include "Core/Profiler.h"
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
			Profiler::BeginFrame();
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
		Log::Initialize(mConfig.log);
		if (mConfig.crashHandler)
		{
			CrashHandler::Settings crash = mConfig.crash;
			crash.directory = mConfig.log.directory;
			CrashHandler::Install(crash);
			// レポートに載せる情報（落ちたときに呼ばれる）
			CrashHandler::SetContext("Engine", [] { return std::format("{} {}{}", kName, kVersion, RD_EDITOR ? "" : " (Game)"); });
			CrashHandler::SetContext("Frame", [this] { return std::to_string(mFrameCount); });
			CrashHandler::SetContext("Scene", [this] { return mScenes.HasScene() ? mScenes.GetScene().GetPath() : std::string("(none)"); });
			CrashHandler::SetContext("Playing", [this] { return mPlay.IsPlaying() ? std::string("yes") : std::string("no"); });
			CrashHandler::SetContext("GPU", [this] { return mDevice ? mDevice->GetAdapterName() : std::string("(not created)"); });
		}
		CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		Profiler::SetThreadName("Main");
		if (mConfig.profileFrames > 0) { Profiler::RequestCapture(mConfig.profileFrames, mConfig.profilePath); }
		Log::Info(LogCat::Core, "{} {}{}", kName, kVersion, RD_EDITOR ? "" : " (Game)");
		Log::Debug(LogCat::Core, "Log file: {}", Log::GetFilePath());

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
			mEditor = std::make_unique<Editor>(*this);
			mEditor->Initialize();
			mWindow.SetMessageHook([this](HWND h, UINT m, WPARAM w, LPARAM l) { return mEditor->HandleMessage(h, m, w, l); });
			mComposer.SetOverlay(mEditor.get());
		}
#endif

		// シーンを差し替える前に、GPU が古いシーンのリソースを使い終わるのを待ち、音を止める
		mScenes.SetBeforeSwap([this]
			{
				mDevice->WaitIdle();
				mAudio->StopAll();
			});

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
		Log::Info(LogCat::Core, "Shutdown complete");
		Log::Shutdown();
	}

	void Engine::Frame(float dt)
	{
		RD_PROFILE_SCOPE("Frame");
		mStats.Tick(dt);

		{
			RD_PROFILE_SCOPE("Main Thread Tasks");
			JobSystem::ExecuteMainThreadTasks();
			ApplyPendingScene();
		}

		PlayController::FrameTime time = mPlay.AdvanceTime(dt);

		mDevice->BeginFrame();

#if RD_EDITOR
		// エディタ UI（ビューポートの大きさやカメラもここで決まる。Stop でシーンが差し替わることもある）
		if (mEditor)
		{
			RD_PROFILE_SCOPE("Editor");
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
		{
			RD_PROFILE_SCOPE("Scene Update");
			scene.Update(time.advance ? time.gameDt : dt, time.advance);
		}
		{
			RD_PROFILE_SCOPE("Audio");
			UpdateAudio(scene);
		}

		// 描画 → バックバッファへ
		mCapture.BeforeRender(*mRenderer, mFrameCount);
		{
			RD_PROFILE_SCOPE("Render");
			mComposer.Render(scene, float(Time::RealTime()));
		}
		mDevice->EndFrame();

		if (mCapture.AfterPresent(*mRenderer, mFrameCount))
		{
			Log::Info(LogCat::Core, "Capture finished, quitting");
			Quit();
		}
		++mFrameCount;
	}

	void Engine::UpdateAudio(Scene& scene)
	{
		// リスナーはゲームカメラ
		if (CameraComponent* mainCam = CameraComponent::FindMain(scene))
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
		Log::Info(LogCat::Core, "Play");
	}

	void Engine::EndPlay()
	{
		if (!mPlay.IsPlaying() || !mEditor) { return; }
		mAudio->StopAll();
		mScenes.SetScene(mPlay.End(mScenes.GetScene()));
		mEditor->OnSceneChanged(true);
		Log::Info(LogCat::Core, "Stop");
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
