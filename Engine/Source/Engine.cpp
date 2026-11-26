#include "RdPch.h"
#include "Engine.h"
#include "Asset/AssetManager.h"
#include "Audio/AudioSystem.h"
#include "Components/Rendering/CameraComponent.h"
#include "Components/UI/UIFont.h"
#include "Core/Debug/CrashHandler.h"
#include "Core/Jobs/JobSystem.h"
#include "Core/Profiling/ProfileScope.h"
#include "Core/Time/Time.h"
#include "Core/Time/Timer.h"
#include "Platform/Input/Input.h"
#include "Render/Renderer/Renderer.h"
#include "RHI/Device/GraphicsDevice.h"
#include "Runtime/BuiltinComponents.h"
#include "Runtime/EngineLayer.h"
#include "Scene/Scene.h"

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
			// デバッガで止めたときなどの大きすぎる dt を防ぐ
			float dt = std::min(timer.Tick(), 0.1f);
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
		Logger::Initialize(mConfig.log);
		if (mConfig.crashHandler)
		{
			CrashHandler::Settings crash = mConfig.crash;
			crash.directory = mConfig.log.directory;
			CrashHandler::Install(crash);
			CrashHandler::SetContext("Engine", [] { return std::format("{} {}{}", kName, kVersion, RD_EDITOR ? "" : " (Game)"); });
			CrashHandler::SetContext("Frame", [this] { return std::to_string(mFrameCount); });
			CrashHandler::SetContext("Scene", [this] { return mScenes.HasScene() ? mScenes.GetScene().GetPath() : std::string("(none)"); });
			CrashHandler::SetContext("Playing", [this] { return mPlay.IsPlaying() ? std::string("yes") : std::string("no"); });
			CrashHandler::SetContext("GPU", [this] { return mDevice ? mDevice->GetAdapterName() : std::string("(not created)"); });
		}
		CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		Profiler::SetThreadName("Main");
		if (mConfig.profileFrames > 0) { Profiler::RequestCapture(mConfig.profileFrames, mConfig.profilePath); }
		Logger::Info(LogCat::Core, "{} {}{}", kName, kVersion, RD_EDITOR ? "" : " (Game)");
		Logger::Debug(LogCat::Core, "Log file: {}", Logger::GetFilePath());

		JobSystem::Initialize();

		// 差し込む層（エディタ）は窓を作る前に作り、窓の位置や最初のシーンを決めさせる
		WindowDesc wd = mConfig.window;
		if (wd.title.empty()) { wd.title = kName; }
		if (mConfig.createLayer) { mLayer = mConfig.createLayer(*this); }
		if (mLayer) { mLayer->OnConfigure(mConfig, wd); }
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
		if (mConfig.deferred) { mRenderer->GetSettings().path = RenderPath::Deferred; }
		mAudio = std::make_unique<AudioSystem>();
		mAudio->Initialize();

		RegisterBuiltinComponents();

		mComposer.Initialize(mWindow, *mDevice, *mRenderer);
		mCapture.Configure(mConfig.captureFrame, mConfig.capturePath);

		// シーンを差し替える前に、GPU が古いシーンのリソースを使い終わるのを待ち、音を止める
		mScenes.SetBeforeSwap([this]
			{
				mDevice->WaitIdle();
				mAudio->StopAll();
			});
		mScenes.CreateInitial(mConfig.startScene, mConfig.buildDefaultScene);

		if (mLayer)
		{
			mLayer->OnInitialize();
			mWindow.SetMessageHook([this](HWND h, UINT m, WPARAM w, LPARAM l) { return mLayer->HandleMessage(h, m, w, l); });
			mComposer.SetOverlay(mLayer.get());
		}
		else
		{
			mPlay.Start();
		}
		return true;
	}

	void Engine::Shutdown()
	{
		// 層（エディタ）は、シーンや窓が残っているうちに後片付けをする（状態の保存など）
		if (mLayer) { mLayer->OnShutdown(); }
		if (mDevice && GraphicsDevice::IsAlive()) { mDevice->WaitIdle(); }
		// DestroyWindow 中のメッセージが破棄済みの層 / デバイスへ届かないように外す
		mWindow.SetMessageHook(nullptr);
		mWindow.SetResizeCallback(nullptr);
		mComposer.SetOverlay(nullptr);
		mScenes.Clear();
		mLayer.reset();
		UIFont::ReleaseAll();
		if (mAudio) { mAudio->Shutdown(); mAudio.reset(); }
		if (mRenderer) { mRenderer->Shutdown(); mRenderer.reset(); }
		if (mAssets) { mAssets->Shutdown(); mAssets.reset(); }
		JobSystem::Shutdown();
		if (mDevice) { mDevice->Shutdown(); mDevice.reset(); }
		mWindow.Destroy();
		CoUninitialize();
		Logger::Info(LogCat::Core, "Shutdown complete");
		Logger::Shutdown();
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

		// 層（エディタ）の更新。ビューポートの大きさやカメラもここで決まる（Stop でシーンが差し替わることもある）
		if (mLayer)
		{
			RD_PROFILE_SCOPE("Editor");
			mLayer->OnBeginFrame();
			mLayer->OnUpdate(dt);
		}
		else
		{
			Input::SetBlocked(false, false);
		}

		Scene& scene = mScenes.GetScene();
		{
			RD_PROFILE_SCOPE("Scene Update");
			scene.Update(time.advance ? time.gameDt : dt, time.advance);
		}
		{
			RD_PROFILE_SCOPE("Audio");
			UpdateAudio(scene);
		}

		mCapture.BeforeRender(*mRenderer, mFrameCount);
		{
			RD_PROFILE_SCOPE("Render");
			mComposer.Render(scene, float(Time::RealTime()));
		}
		mDevice->EndFrame();

		if (mCapture.AfterPresent(*mRenderer, mFrameCount))
		{
			Logger::Info(LogCat::Core, "Capture finished, quitting");
			Quit();
		}
		++mFrameCount;
	}

	void Engine::UpdateAudio(Scene& scene)
	{
		// 音を聞く位置はゲームのカメラ
		if (CameraComponent* mainCam = CameraComponent::FindMain(scene))
		{
			mAudio->SetListener(mainCam->GetTransform().GetPosition(), mainCam->GetTransform().GetRight());
		}
		mAudio->Update();
	}

	// プレイ中にゲームが切り替えた場面は、そのままプレイを続ける（Stop でプレイを始めた場面に戻る）。
	//  エディタで開く・新しく作るときは、先にエディタがプレイを止めてから切り替える
	void Engine::ApplyPendingScene()
	{
		if (!mScenes.ApplyPending()) { return; }
		if (mLayer) { mLayer->OnSceneChanged(); }
	}
}
