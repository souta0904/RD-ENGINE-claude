#pragma once
#include <functional>
#include <memory>
#include <string>

namespace rd
{
	class Scene;

	// 現在のシーンの所有と切り替え
	//  LoadScene / NewScene / ReplaceScene は保留され、フレームの区切りの ApplyPending で差し替わる。
	class SceneManager
	{
	public:
		SceneManager();
		~SceneManager();

		// 起動時のシーン（path のファイルがあれば読み、なければ build で組み立てる）
		void CreateInitial(const std::string& path, const std::function<void(Scene&)>& build);

		Scene& GetScene() { return *mScene; }
		bool HasScene() const { return mScene != nullptr; }

		// シーン切り替えの予約（フレームの区切りで行われる）
		void LoadScene(const std::string& path);
		void NewScene();
		void ReplaceScene(std::unique_ptr<Scene> scene);

		// 予約された切り替えを反映する。切り替えたら true
		bool ApplyPending();
		// GPU の完了を待ってから即座に差し替える
		void SetScene(std::unique_ptr<Scene> scene);
		// すべてのシーンを破棄（終了時）
		void Clear();

	private:
		std::unique_ptr<Scene> mScene;
		std::unique_ptr<Scene> mPendingScene;
		std::string mPendingScenePath;
	};
}
