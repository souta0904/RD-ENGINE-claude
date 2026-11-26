#pragma once

namespace rd
{
	class Editor;

	// パネルの外側の画面: 上のメニューバーとツールバー、下のステータスバー、全体のショートカット
	//  何をするかはエディタの各部品（ActorActions・SceneDocument・PlayMode・EditorUndo など）に任せ、ここは並べて呼ぶだけ
	class RD_API EditorMainUI
	{
	public:
		explicit EditorMainUI(Editor& editor) : mEditor(editor) {}

		// パネルより先に描く（メニュー・ツールバー・ステータスバーの分だけ、パネルの場所が狭くなる）
		void DrawBars();
		// パネルを描いた後に呼ぶ
		void HandleShortcuts();
		void DrawDemoWindow();

	private:
		void DrawMenuBar();
		void DrawLanguageMenu();
		void DrawMainToolbar();
		void DrawCompileButton();
		void DrawStatusBar();

		Editor& mEditor;
		bool mShowDemo = false;
	};
}
