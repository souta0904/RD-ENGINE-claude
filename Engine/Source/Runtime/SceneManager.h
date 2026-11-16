#pragma once
#include <functional>
#include <memory>
#include <string>

namespace rd
{
	class Scene;

	// 今のシーンを持ち、切り替える
	//  LoadScene / NewScene / ReplaceScene は予約だけして、フレームの区切りの ApplyPending で差し替える
	//  差し替える直前に BeforeSwap が呼ばれる（GPU の完了待ちや音の停止。エンジンが設定する）
	class SceneManager
	{
	public:
		SceneManager();
		~SceneManager();

		void SetBeforeSwap(std::function<void()> callback) { mBeforeSwap = std::move(callback); }

		// path のファイルがあれば読み、なければ build で組み立てる
		void CreateInitial(const std::string& path, const std::function<void(Scene&)>& build);

		Scene& GetScene() { return *mScene; }
		bool HasScene() const { return mScene != nullptr; }

		void LoadScene(const std::string& path);
		void NewScene();
		void ReplaceScene(std::unique_ptr<Scene> scene);

		// 差し替えたら true
		bool ApplyPending();
		// すぐに差し替える（BeforeSwap は呼ぶ）
		void SetScene(std::unique_ptr<Scene> scene);
		void Clear();

	private:
		std::unique_ptr<Scene> mScene;
		std::unique_ptr<Scene> mPendingScene;
		std::string mPendingScenePath;
		std::function<void()> mBeforeSwap;
	};
}
