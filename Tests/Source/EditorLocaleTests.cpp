#include "TestFramework.h"
#if RD_EDITOR
#include "Editor/Core/EditorLocale.h"
#include <filesystem>

using namespace rd;

namespace
{
	// テストの後は言語を元に戻す（ほかのテストに影響しないように）
	struct LanguageScope
	{
		EditorLocale::Language saved = EditorLocale::GetLanguage();
		explicit LanguageScope(EditorLocale::Language lang) { EditorLocale::SetLanguage(lang); }
		~LanguageScope() { EditorLocale::SetLanguage(saved); }
	};
}

// 英語を選んでいるときは、渡した文字列がそのまま返る
RD_TEST(EditorLocale, EnglishPassesThrough)
{
	LanguageScope scope(EditorLocale::Language::English);
	RD_CHECK_EQ(std::string(Tr("File")), std::string("File"));
	RD_CHECK_EQ(std::string(Tr("Rename##project")), std::string("Rename##project"));
	RD_CHECK_EQ(EditorLocale::Format("Undo {}", 3), std::string("Undo 3"));
	RD_CHECK(!EditorLocale::Has("File"));
}

// TrId は「表示###英語」の形（ImGui の ID が英語のまま保たれる）
RD_TEST(EditorLocale, TrIdKeepsEnglishId)
{
	LanguageScope scope(EditorLocale::Language::English);
	RD_CHECK_EQ(std::string(TrId("Scene")), std::string("Scene###Scene"));
}

// 表にない言葉は英語のまま。"##" から後ろは訳さずに付け直す。同じ文字列には同じポインタが返る
RD_TEST(EditorLocale, JapaneseTable)
{
	LanguageScope scope(EditorLocale::Language::Japanese);
	RD_CHECK_EQ(std::string(Tr("No Such Words In The Table")), std::string("No Such Words In The Table"));
	RD_CHECK(Tr("Some Label##id") == Tr("Some Label##id"));
	if (!std::filesystem::exists("Assets/Engine/Localization/ja.json")) { return; } // 作業フォルダが違うときは表を読めない
	RD_CHECK(EditorLocale::Has("File"));
	RD_CHECK_EQ(std::string(Tr("File")), std::string("ファイル"));
	RD_CHECK_EQ(std::string(Tr("Rename##project")), std::string("名前を変更##project"));
	RD_CHECK_EQ(std::string(TrId("Scene")), std::string("シーン###Scene"));
	RD_CHECK_EQ(EditorLocale::Format("Undo {}", std::string("X")), std::string("元に戻す X"));
}
#endif
