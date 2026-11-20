#pragma once
#include "Editor/Style/EditorIcons.h"
#include <string>

namespace rd
{
	// どのパネルでも同じ見た目になるよう、よく使う UI の部品をまとめたもの
	//  パネルの上のツールバーは BeginToolbar / EndToolbar で囲み、その中にボタンや検索欄を横に並べる
	class RD_API EditorGui
	{
	public:
		static void BeginToolbar(const char* id);
		static void EndToolbar();
		// アイコンだけのボタン（active なら押された状態で表示）
		static bool IconButton(const char* id, Icon icon, const char* tooltip, bool active = false, bool enabled = true);
		// アイコンと文字のボタン
		static bool IconTextButton(const char* id, Icon icon, const char* label, const char* tooltip = nullptr);
		// 文字のボタン（active なら押された状態で表示）
		static bool ToggleButton(const char* label, bool active);
		// width <= 0 なら残りの幅いっぱい
		static bool SearchField(const char* id, std::string& text, const char* hint = "Search", float width = 0.0f);
		// 縦の区切り（ツールバーの中のまとまりを分ける）
		static void ToolbarSeparator();
		// 何も表示するものがないときの案内
		static void EmptyHint(const char* text);
		// 見出しの付いた区切り
		static void SectionLabel(const char* text);
		// 「保存しますか？」などの確認。true を返したら閉じた（choice に 0..n-1 か、取り消しなら -1）
		static bool ConfirmPopup(const char* id, const char* message, const char* const* buttons, int buttonCount, int& choice);
	};
}
