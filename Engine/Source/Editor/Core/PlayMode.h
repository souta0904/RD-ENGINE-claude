#pragma once
#include <string>

namespace rd
{
	class Editor;

	// エディタのプレイ（Play / Stop / Pause / Step）
	//  Play でシーンを JSON に写して取っておき、Stop でその写しからシーンを作り直す（Unity と同じく、プレイ中の変更は残らない）。
	//  時間の進め方と一時停止はエンジンの PlayController が受け持つ
	class RD_API PlayMode
	{
	public:
		explicit PlayMode(Editor& editor) : mEditor(editor) {}

		bool IsPlaying() const;
		bool IsPaused() const;

		void Begin();
		// プレイを始めた場面に戻す（プレイ中でなければ何もしない）
		void End();
		void Toggle();
		void TogglePause();
		// 一時停止して 1 フレームだけ進める
		void Step();

		// プレイを始めたときのシーンのパス（プレイ中にゲームが場面を変えても、エディタで開いているのはこれ）
		const std::string& GetStartScenePath() const { return mSnapshotPath; }

	private:
		Editor& mEditor;
		std::string mSnapshot;
		std::string mSnapshotPath;
	};
}
