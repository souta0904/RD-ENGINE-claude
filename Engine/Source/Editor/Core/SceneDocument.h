#pragma once
#include <functional>
#include <string>

namespace rd
{
	class Editor;

	// 編集しているシーンのファイル（新しく作る・開く・保存する・エディタを閉じる）
	//  保存していない変更があれば、別のシーンへ移る前・閉じる前に「保存しますか？」と聞く
	class RD_API SceneDocument
	{
	public:
		explicit SceneDocument(Editor& editor) : mEditor(editor) {}

		void NewScene();
		// ファイルを選ぶ画面を出して開く
		void OpenScene();
		void OpenScene(const std::string& path);
		// saveAs か、まだ一度も保存していなければ保存先を聞く。保存できたら true
		bool SaveScene(bool saveAs);
		// エディタを閉じる（保存していない変更があれば先に聞く）
		void RequestQuit();
		// 窓を閉じようとしたとき: 先に聞く必要があれば聞いて true（閉じるのを止める）
		bool InterceptClose();

		// 「保存しますか？」の画面（毎フレーム呼ぶ）
		void DrawSavePrompt();

	private:
		// 保存していない変更があれば「保存しますか？」を出してから action を実行する
		void RunAfterSaveCheck(std::function<void()> action);

		Editor& mEditor;
		std::function<void()> mPendingAfterSave;
		bool mOpenSavePrompt = false;
		bool mAllowClose = false;
	};
}
