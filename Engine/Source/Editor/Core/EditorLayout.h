#pragma once

namespace rd
{
	class Editor;

	// パネルの並べ方（ドッキング）
	//  配置は ImGui が Logs/editor_layout.ini に保存する。保存がない・既定の配置の版が古いときは既定の配置を作る
	class RD_API EditorLayout
	{
	public:
		explicit EditorLayout(Editor& editor) : mEditor(editor) {}

		// ImGui を初期化した後に呼ぶ（保存先と、既定の配置を作るかを決める）
		void Initialize();
		// 毎フレーム、パネルを描く前に呼ぶ（画面全体をドッキングの場所にする）
		void BeginDockSpace();
		// パネルを描いた後に呼ぶ（既定の配置を作ったときのタブの選び直し）
		void AfterPanels();
		// 次のフレームで既定の配置に作り直す
		void Reset() { mRebuild = true; }

	private:
		void Build(unsigned int dockId);
		void SelectDefaultTabs();

		Editor& mEditor;
		bool mRebuild = false;
		bool mSelectDefaultTabs = false;
	};
}
