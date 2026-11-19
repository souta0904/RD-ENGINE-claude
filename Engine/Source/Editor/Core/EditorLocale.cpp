#include "RdPch.h"
#include "Editor/Core/EditorLocale.h"
#include "Core/Serialization/Json.h"
#include "Core/Util/FileUtil.h"
#include "Core/Util/StringUtil.h"
#include <filesystem>
#include <fstream>
#include <set>
#include <unordered_map>

namespace rd::EditorLocale
{
	namespace
	{
		// 文字列の表。探すときに一時的な std::string を作らないよう、string_view でも引けるようにする
		struct StringHash
		{
			using is_transparent = void;
			size_t operator()(std::string_view s) const { return std::hash<std::string_view>{}(s); }
		};
		using Table = std::unordered_map<std::string, std::string, StringHash, std::equal_to<>>;

		struct State
		{
			Language language = Language::Japanese;
			bool loaded = false;
			Table table;     // 英語 → 訳（読み込んだ JSON の中身）
			Table resolved;  // Tr に渡された文字列 → 返す文字列（"##" の後ろを付け直したもの。言語ごとに作り直す）
			Table withId;    // TrId の結果
			std::set<std::string> missing;
		};

		State& Get()
		{
			static State state;
			return state;
		}

		// Assets/Engine を先に読み、ゲームのフォルダの表で上書きできるようにする
		void LoadTables(State& s)
		{
			s.table.clear();
			s.resolved.clear();
			s.withId.clear();
			s.missing.clear();
			s.loaded = true;
			if (s.language == Language::English) { return; }
			std::string file = std::format("Localization/{}.json", GetLanguageCode(s.language));
			std::vector<std::string> paths = { "Assets/Engine/" + file };
			std::error_code ec;
			for (const auto& entry : std::filesystem::directory_iterator(L"Assets", ec))
			{
				if (!entry.is_directory(ec)) { continue; }
				std::string folder = ToUtf8(entry.path().filename().wstring());
				if (folder == "Engine") { continue; }
				std::string path = "Assets/" + folder + "/" + file;
				if (std::filesystem::exists(ToWide(path), ec)) { paths.push_back(path); }
			}
			for (const std::string& path : paths)
			{
				Json json;
				if (!LoadJsonFile(path, json) || !json.is_object()) { continue; }
				// { "グループの名前": { "英語": "訳", ... }, ... } のように分けて書いてもよい（読みやすさのため）
				auto add = [&](const Json& obj, auto&& self) -> void
					{
						for (auto it = obj.begin(); it != obj.end(); ++it)
						{
							if (it.value().is_string()) { s.table[it.key()] = it.value().get<std::string>(); }
							else if (it.value().is_object()) { self(it.value(), self); }
						}
					};
				add(json, add);
			}
		}

		State& Ready()
		{
			State& s = Get();
			if (!s.loaded) { LoadTables(s); }
			return s;
		}

		// "##" の前を訳し、後ろはそのまま付ける
		std::string Translate(State& s, std::string_view english)
		{
			size_t hash = english.find("##");
			std::string_view head = hash == std::string_view::npos ? english : english.substr(0, hash);
			std::string_view tail = hash == std::string_view::npos ? std::string_view() : english.substr(hash);
			if (head.empty() || s.language == Language::English) { return std::string(english); }
			auto it = s.table.find(head);
			if (it == s.table.end())
			{
				s.missing.emplace(head);
				return std::string(english);
			}
			return it->second + std::string(tail);
		}
	}

	void SetLanguage(Language language)
	{
		State& s = Get();
		if (s.loaded && s.language == language) { return; }
		s.language = language;
		LoadTables(s);
	}

	Language GetLanguage()
	{
		return Get().language;
	}

	const char* GetLanguageCode(Language language)
	{
		return language == Language::Japanese ? "ja" : "en";
	}

	Language FromCode(std::string_view code)
	{
		return code == "en" ? Language::English : Language::Japanese;
	}

	void Reload()
	{
		LoadTables(Get());
	}

	const char* Tr(std::string_view english)
	{
		State& s = Ready();
		auto it = s.resolved.find(english);
		if (it == s.resolved.end()) { it = s.resolved.emplace(std::string(english), Translate(s, english)).first; }
		return it->second.c_str();
	}

	const char* TrId(std::string_view english)
	{
		State& s = Ready();
		auto it = s.withId.find(english);
		if (it == s.withId.end()) { it = s.withId.emplace(std::string(english), std::string(Tr(english)) + "###" + std::string(english)).first; }
		return it->second.c_str();
	}

	bool Has(std::string_view english)
	{
		State& s = Ready();
		return s.language != Language::English && s.table.contains(english);
	}

	void WriteMissing()
	{
		State& s = Get();
		if (s.language == Language::English) { return; }
		std::error_code ec;
		std::string path = std::format("Logs/missing_{}.txt", GetLanguageCode(s.language));
		// 訳し漏れがなければ、前に書いた一覧を消す（古い一覧が残っていると紛らわしい）
		if (s.missing.empty()) { std::filesystem::remove(path, ec); return; }
		std::filesystem::create_directories("Logs", ec);
		std::ofstream file(path, std::ios::binary);
		for (const std::string& m : s.missing) { file << m << "\n"; }
	}
}
