#pragma once
#include <ImGui/imgui.h>

namespace rd
{
	class Editor;

	// エディタのパネルの基底（ドッキングできるウィンドウ 1 つ）
	//  Update は閉じていても毎フレーム呼ばれ、OnGui は開いているときだけウィンドウの中で呼ばれる。
	//  ウィンドウの開始と終了・余白などの見た目はここでそろえる
	class EditorPanel
	{
	public:
		EditorPanel(Editor& editor, const char* title, bool open = true) : mEditor(editor), mTitle(title), mOpen(open) {}
		virtual ~EditorPanel() = default;
		EditorPanel(const EditorPanel&) = delete;
		EditorPanel& operator=(const EditorPanel&) = delete;

		const char* GetTitle() const { return mTitle; }
		bool IsOpen() const { return mOpen; }
		void SetOpen(bool open) { mOpen = open; }
		bool* GetOpenFlag() { return &mOpen; }
		// メニューの Window に出す名前の横のショートカット（なければ nullptr）
		virtual const char* GetShortcut() const { return nullptr; }

		virtual void Update(float /*dt*/) {}
		void Draw();

	protected:
		virtual void OnGui() = 0;
		virtual ImGuiWindowFlags GetWindowFlags() const { return ImGuiWindowFlags_None; }
		// false ならウィンドウの余白をなくす（ビューポートなど）
		virtual bool HasPadding() const { return true; }
		virtual ImVec2 GetDefaultSize() const { return ImVec2(400.0f, 300.0f); }

		Editor& mEditor;

	private:
		const char* mTitle;
		bool mOpen;
	};
}
