#pragma once
#include "Platform/Window.h"
#include <functional>
#include <memory>
#include <string>

namespace rd
{
	class AssetManager;
	class AudioSystem;
	class Editor;
	class GraphicsDevice;
	class Renderer;
	class Scene;

	struct EngineConfig
	{
		WindowDesc window;
		bool enableEditor = true;
		bool vsync = true;
		std::string startScene;// 起動時に開くシーン
		// startScene が存在しないときに初期シーンを組み立てる
		std::function<void(Scene&)> buildDefaultScene;

		// 自動テスト用: 指定フレームで画面を保存して終了
		int captureFrame = -1;
		std::string capturePath = "Logs/capture.png";
		bool autoPlay = false;// エディタでも起動直後にプレイ開始

		// コマンドライン（-noeditor -play -capture <frame> <path> -scene <path>）を反映
		void ParseCommandLine();
	};

	// エンジン本体（サブシステムの所有者とメインループ）
	class Engine
	{
	public:
		static constexpr const char* kName = "RD ENGINE";
		static constexpr const char* kVersion = "2.0.0";

		static Engine& Get() { return *sInstance; }

		Engine();
		~Engine();
		int Run(const EngineConfig& config);
		void Quit();

		Window& GetWindow() { return mWindow; }
		Scene& GetScene() { return *mScene; }
		Editor* GetEditor() { return mEditor.get(); }
		const EngineConfig& GetConfig() const { return mConfig; }

		// プレイ状態（エディタなしでは常にプレイ中）
		bool IsPlaying() const { return mPlaying; }
		bool IsPaused() const { return mPaused; }
		void SetPaused(bool paused) { mPaused = paused; }
		void StepFrame() { mStepRequested = true; }
		void BeginPlay();
		void EndPlay();

		// シーン切り替え（フレームの区切りで行われる）
		void LoadScene(const std::string& path);
		void NewScene();
		void ReplaceScene(std::unique_ptr<Scene> scene);

		float GetFps() const { return mFps; }
		float GetFrameMs() const { return mFrameMs; }
		void SetWindowTitleSuffix(const std::string& suffix);

	private:
		bool Initialize();
		void Shutdown();
		void Frame(float dt);
		void ApplyPendingScene();

		static inline Engine* sInstance = nullptr;
		EngineConfig mConfig;
		Window mWindow;
		std::unique_ptr<GraphicsDevice> mDevice;
		std::unique_ptr<AssetManager> mAssets;
		std::unique_ptr<Renderer> mRenderer;
		std::unique_ptr<AudioSystem> mAudio;
		std::unique_ptr<Editor> mEditor;
		std::unique_ptr<Scene> mScene;
		std::unique_ptr<Scene> mPendingScene;
		std::string mPendingScenePath;

		bool mRunning = true;
		bool mPlaying = false;
		bool mPaused = false;
		bool mStepRequested = false;
		std::string mPlaySnapshot;// プレイ開始時のシーン（停止時に戻す）
		uint64_t mFrameCount = 0;
		float mFps = 0.0f;
		float mFrameMs = 0.0f;
	};

	// エンジン組み込みコンポーネントを登録
	void RegisterBuiltinComponents();
}
