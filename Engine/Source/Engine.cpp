#include "RdPch.h"
#include "Engine.h"
#include "Asset/AssetManager.h"
#include "Audio/AudioSystem.h"
#include "Components/Animator.h"
#include "Components/AudioSource.h"
#include "Components/CameraComponent.h"
#include "Components/Lights.h"
#include "Components/MeshRenderer.h"
#include "Components/ParticleSystem.h"
#include "Components/SpriteRenderer.h"
#include "Core/JobSystem.h"
#include "Core/StringUtil.h"
#include "Core/Time.h"
#include "Core/Timer.h"
#include "Editor/Editor.h"
#include "Physics/Collider.h"
#include "Physics/Rigidbody.h"
#include "Platform/Input.h"
#include "Render/Renderer.h"
#include "RHI/GraphicsDevice.h"
#include "Scene/ComponentRegistry.h"
#include "Scene/Scene.h"
#include <shellapi.h>

namespace rd
{
	void RegisterBuiltinComponents()
	{
		ComponentRegistry::Register<MeshRenderer>("Rendering");
		ComponentRegistry::Register<SpriteRenderer>("Rendering");
		ComponentRegistry::Register<ParticleSystem>("Rendering");
		ComponentRegistry::Register<CameraComponent>("Rendering");
		ComponentRegistry::Register<DirectionalLight>("Light");
		ComponentRegistry::Register<PointLight>("Light");
		ComponentRegistry::Register<SpotLight>("Light");
		ComponentRegistry::Register<Animator>("Animation");
		ComponentRegistry::Register<Rigidbody>("Physics");
		ComponentRegistry::Register<SphereCollider>("Physics");
		ComponentRegistry::Register<BoxCollider>("Physics");
		ComponentRegistry::Register<CapsuleCollider>("Physics");
		ComponentRegistry::Register<MeshCollider>("Physics");
		ComponentRegistry::Register<AudioSource>("Audio");
	}

	// ==================================================
	// EngineConfig
	// ==================================================
	void EngineConfig::ParseCommandLine()
	{
		int argc = 0;
		LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
		for (int i = 1; i < argc; ++i)
		{
			std::string arg = ToUtf8(argv[i]);
			if (arg == "-noeditor") { enableEditor = false; }
			else if (arg == "-play") { autoPlay = true; }
			else if (arg == "-novsync") { vsync = false; }
			else if (arg == "-scene" && i + 1 < argc) { startScene = ToUtf8(argv[++i]); }
			else if (arg == "-capture" && i + 1 < argc)
			{
				captureFrame = std::stoi(ToUtf8(argv[++i]));
				if (i + 1 < argc && argv[i + 1][0] != L'-') { capturePath = ToUtf8(argv[++i]); }
			}
		}
		LocalFree(argv);
	}

	// ==================================================
	// Engine
	// ==================================================
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
		Log::Info("{} {}", kName, kVersion);

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

		if (mConfig.enableEditor)
		{
			mEditor = std::make_unique<Editor>();
			mEditor->Initialize();
			mWindow.SetMessageHook([this](HWND h, UINT m, WPARAM w, LPARAM l) { return mEditor->HandleMessage(h, m, w, l); });
		}

		// 初期シーン
		mScene = std::make_unique<Scene>();
		if (!mConfig.startScene.empty() && std::filesystem::exists(ToWide(mConfig.startScene)))
		{
			mScene->LoadFromFile(mConfig.startScene);
		}
		else if (mConfig.buildDefaultScene)
		{
			mConfig.buildDefaultScene(*mScene);
			if (!mConfig.startScene.empty()) { mScene->SetPath(mConfig.startScene); }
		}
		if (mEditor) { mEditor->OnSceneChanged(); }

		mPlaying = !mEditor;
		if (mEditor && mConfig.autoPlay) { BeginPlay(); }
		SetWindowTitleSuffix("");
		return true;
	}

	void Engine::Shutdown()
	{
		if (mDevice && GraphicsDevice::IsAlive()) { mDevice->WaitIdle(); }
		if (mScene) { mScene.reset(); }
		mPendingScene.reset();
		if (mEditor) { mEditor->Shutdown(); mEditor.reset(); }
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
		// FPS 計測
		{
			static float accum = 0.0f;
			static int frames = 0;
			accum += dt;
			++frames;
			if (accum >= 0.5f)
			{
				mFps = float(frames) / accum;
				mFrameMs = accum * 1000.0f / float(frames);
				accum = 0.0f;
				frames = 0;
			}
		}

		JobSystem::ExecuteMainThreadTasks();
		ApplyPendingScene();

		// 時間
		bool advance = mPlaying && (!mPaused || mStepRequested);
		float gameDt = advance ? (mStepRequested ? Time::FixedDeltaTime() : dt) * Time::TimeScale() : 0.0f;
		mStepRequested = false;
		Time::sUnscaledDelta = dt;
		Time::sDelta = gameDt;
		if (advance) { Time::sTotal += gameDt; }
		++Time::sFrameCount;

		auto& device = *mDevice;
		device.BeginFrame();

		// エディタ UI（ビューポートの大きさやカメラもここで決まる）
		if (mEditor)
		{
			mEditor->BeginFrame();
			mEditor->Update(dt);
		}
		else
		{
			Input::SetBlocked(false, false);
		}

		// シーン更新（プレイ中でなければ編集用の更新のみ）
		mScene->Update(advance ? gameDt : dt, advance);

		// カメラを決める
		uint32_t viewW = mWindow.GetWidth(), viewH = mWindow.GetHeight();
		if (mEditor) { mEditor->GetViewportSize(viewW, viewH); }
		float aspect = float(std::max(1u, viewW)) / float(std::max(1u, viewH));
		RenderCamera camera;
		CameraComponent* mainCam = mScene->GetMainCamera();
		if (mEditor && mEditor->UseEditorCamera())
		{
			camera = mEditor->GetEditorCamera(aspect);
		}
		else if (mainCam)
		{
			camera = mainCam->GetRenderCamera(aspect);
		}
		else
		{
			camera = RenderCamera::Make({ 0.0f, 3.0f, -10.0f }, Quaternion::FromEuler({ math::ToRadians(10.0f), 0.0f, 0.0f }),
				math::ToRadians(60.0f), aspect, 0.1f, 1000.0f);
		}

		// オーディオのリスナーはゲームカメラ
		if (mainCam) { mAudio->SetListener(mainCam->GetTransform().GetPosition(), mainCam->GetTransform().GetRight()); }
		mAudio->Update();

		// 描画
		auto& renderer = *mRenderer;
		mScene->CollectRender(renderer.GetRenderList());
		if (mEditor) { mEditor->DrawGizmos(renderer.GetDebugDraw()); }
		if (mConfig.captureFrame >= 0 && int64_t(mFrameCount) == mConfig.captureFrame)
		{
			renderer.RequestCapture(mConfig.capturePath);
		}
		renderer.Render(camera, mScene->GetEnvironment(), viewW, viewH, float(Time::RealTime()));

		// バックバッファへ
		auto cmd = device.GetCommandList();
		TransitionBarrier(cmd, device.GetBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		if (mEditor)
		{
			auto rtv = device.GetBackBufferRtv();
			cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
			float clear[4] = { 0.08f, 0.08f, 0.09f, 1.0f };
			cmd->ClearRenderTargetView(rtv, clear, 0, nullptr);
			SetViewport(cmd, device.GetWidth(), device.GetHeight());
			mEditor->RenderUI();
		}
		else
		{
			renderer.BlitToBackBuffer();
		}
		TransitionBarrier(cmd, device.GetBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		device.EndFrame();

		renderer.FinishCapture();
		if (mConfig.captureFrame >= 0 && int64_t(mFrameCount) == mConfig.captureFrame)
		{
			Log::Info("Capture finished, quitting");
			Quit();
		}
		++mFrameCount;
	}

	// ==================================================
	// プレイ
	// ==================================================
	void Engine::BeginPlay()
	{
		if (mPlaying) { return; }
		mPlaySnapshot = mScene->Serialize().dump();
		mPlaying = true;
		mPaused = false;
		Time::sTotal = 0.0;
		Log::Info("Play");
	}

	void Engine::EndPlay()
	{
		if (!mPlaying || !mEditor) { return; }
		mPlaying = false;
		mPaused = false;
		AudioSystem::Get().StopAll();
		// プレイ前の状態へ戻す
		std::string path = mScene->GetPath();
		auto scene = std::make_unique<Scene>();
		scene->Deserialize(Json::parse(mPlaySnapshot));
		scene->SetPath(path);
		mDevice->WaitIdle();
		mScene = std::move(scene);
		mEditor->OnSceneChanged(true);
		Log::Info("Stop");
	}

	// ==================================================
	// シーン
	// ==================================================
	void Engine::LoadScene(const std::string& path)
	{
		mPendingScenePath = path;
	}

	void Engine::NewScene()
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

	void Engine::ReplaceScene(std::unique_ptr<Scene> scene)
	{
		mPendingScene = std::move(scene);
	}

	void Engine::ApplyPendingScene()
	{
		if (!mPendingScenePath.empty())
		{
			auto scene = std::make_unique<Scene>();
			if (scene->LoadFromFile(mPendingScenePath)) { mPendingScene = std::move(scene); }
			mPendingScenePath.clear();
		}
		if (!mPendingScene) { return; }
		mDevice->WaitIdle();
		AudioSystem::Get().StopAll();
		mScene = std::move(mPendingScene);
		if (mEditor)
		{
			mPlaying = false;
			mEditor->OnSceneChanged();
		}
		SetWindowTitleSuffix("");
	}

	void Engine::SetWindowTitleSuffix(const std::string& suffix)
	{
		std::string scene = mScene ? (mScene->GetPath().empty() ? std::string("Untitled") : mScene->GetPath()) : "";
		mWindow.SetTitle(std::format("{} {} - {}{}", kName, kVersion, scene, suffix));
	}
}
