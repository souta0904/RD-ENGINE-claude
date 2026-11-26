#pragma once
#include "EngineConfig.h"
#include "Core/Time/FrameStats.h"
#include "Platform/Window/Window.h"
#include "Runtime/FrameCapture.h"
#include "Runtime/FrameComposer.h"
#include "Runtime/PlayController.h"
#include "Runtime/SceneManager.h"
#include <memory>
#include <string>

namespace rd
{
	class AssetManager;
	class AudioSystem;
	class EngineLayer;
	class GraphicsDevice;
	class Renderer;
	class Scene;

	// エンジン本体（サブシステムを持ち、初期化と終了の順番とメインループを受け持つ）
	//  シーンの切り替えは SceneManager、プレイの状態は PlayController、描画の組み立ては FrameComposer に任せる。
	//  エディタのことは知らない。エディタは EngineLayer として外から差し込まれ（EngineConfig::createLayer）、
	//  エンジンは決まったときにその関数を呼ぶだけ。層がなければ（Game 構成・-noeditor）いつもプレイ中として動く。
	//  サブシステムは上から順に作り、Shutdown で逆の順に消す
	class RD_API Engine
	{
	public:
		static constexpr const char* kName = "RD ENGINE";
		static constexpr const char* kVersion = "1.0.0";

		static Engine& Get() { return *sInstance; }

		Engine();
		~Engine();
		int Run(const EngineConfig& config);
		void Quit();

		Window& GetWindow() { return mWindow; }
		const EngineConfig& GetConfig() const { return mConfig; }
		SceneManager& GetSceneManager() { return mScenes; }
		PlayController& GetPlayController() { return mPlay; }

		bool IsPlaying() const { return mPlay.IsPlaying(); }
		bool IsPaused() const { return mPlay.IsPaused(); }
		void SetPaused(bool paused) { mPlay.SetPaused(paused); }
		void StepFrame() { mPlay.StepFrame(); }

		// 切り替えはフレームの区切りで行われる
		Scene& GetScene() { return mScenes.GetScene(); }
		void LoadScene(const std::string& path) { mScenes.LoadScene(path); }
		void NewScene() { mScenes.NewScene(); }
		void ReplaceScene(std::unique_ptr<Scene> scene) { mScenes.ReplaceScene(std::move(scene)); }

		float GetFps() const { return mStats.GetFps(); }
		float GetFrameMs() const { return mStats.GetFrameMs(); }
		uint64_t GetFrameCount() const { return mFrameCount; }

	private:
		bool Initialize();
		void Shutdown();
		void Frame(float dt);
		void ApplyPendingScene();
		void UpdateAudio(Scene& scene);

		static inline Engine* sInstance = nullptr;
		EngineConfig mConfig;

		Window mWindow;
		std::unique_ptr<GraphicsDevice> mDevice;
		std::unique_ptr<AssetManager> mAssets;
		std::unique_ptr<Renderer> mRenderer;
		std::unique_ptr<AudioSystem> mAudio;
		// 外から差し込まれた層（エディタ）。なければ nullptr
		std::unique_ptr<EngineLayer> mLayer;

		SceneManager mScenes;
		PlayController mPlay;
		FrameComposer mComposer;
		FrameCapture mCapture;
		FrameStats mStats;

		bool mRunning = true;
		uint64_t mFrameCount = 0;
	};
}
