#pragma once
#include <format>
#include <string>
#include <string_view>

namespace rd
{
	// エディタの表示の言語（Unity の Preferences > Languages にあたる）
	//
	//  コードの中の文字列は英語のまま書き、表示するときに Tr("Save") のように通すと、
	//  選んでいる言語の訳に置き換わる。訳は JSON の表（英語 → 訳）に書く:
	//    Assets/Engine/Localization/ja.json  … エンジンとエディタの言葉
	//    Assets/<ゲーム>/Localization/ja.json … ゲームのコンポーネントの項目名など（Assets 直下のフォルダを全部探す）
	//  表は起動時と言語を切り替えたときに読むので、訳を直したらエディタの Window > Language で選び直せば反映される。
	//  表にない言葉は英語のまま出す（訳し忘れても表示が消えることはない）。
	//
	//  ImGui は表示の文字列を部品の ID にも使うので、訳すと ID が変わる。
	//  ウィンドウのように ID を保存するもの（ドッキングの配置）は TrId で「訳###英語」にして、ID を英語のまま保つ
	namespace EditorLocale
	{
		enum class Language { English, Japanese };

		void SetLanguage(Language language);
		Language GetLanguage();
		const char* GetLanguageCode(Language language); // "en" / "ja"
		Language FromCode(std::string_view code);

		// 表を読み直す（言語を選び直したとき）
		void Reload();

		// 英語の文字列の訳。"##" から後ろ（ImGui の ID の部分）は訳さずに付け直す。
		//  返す文字列はエディタが終わるまで消えない（毎フレーム同じ文字列を探しても増えない）
		const char* Tr(std::string_view english);
		// "訳###英語"（ウィンドウやタブのように、言語を変えても同じ部品として扱わせたいもの）
		const char* TrId(std::string_view english);
		// 表にあるか（項目名の欄で、訳したときだけ元の名前を乗せたときに出す、などに使う）
		bool Has(std::string_view english);

		// 訳した書式で std::format する（訳でも {} の数と並びは同じにする）
		template <class... Args>
		std::string Format(std::string_view englishFormat, Args&&... args)
		{
			return std::vformat(Tr(englishFormat), std::make_format_args(args...));
		}

		// 表になかった言葉を Logs/missing_<言語>.txt に書き出す（訳の書き足しに使う。終了時に呼ぶ）
		void WriteMissing();
	}

	// よく使うので短い名前も置く
	inline const char* Tr(std::string_view english) { return EditorLocale::Tr(english); }
	inline const char* TrId(std::string_view english) { return EditorLocale::TrId(english); }
}
