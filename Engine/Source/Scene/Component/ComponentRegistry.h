#pragma once
#include "Scene/Component/Component.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace rd
{
	// 型名からコンポーネントを作るための一覧
	//  エンジンの組み込みは Engine の初期化で登録される。
	//  ゲーム側は .cpp に RD_REGISTER_COMPONENT(MyComponent, "Game") と書けば自動で登録される。
	//
	//  ゲームのコードを読み直すとき（GameModule）は、ゲームが登録したものをいったん外し、新しい DLL が
	//  読み込まれたときに登録し直させる。どちらが登録したかを見分けるため、登録するときの「読み込み中の
	//  モジュールの番号」を Entry に覚えておく（エンジンは 0、ゲームの DLL は 1 から）
	class RD_API ComponentRegistry
	{
	public:
		struct Entry
		{
			std::string name;
			std::string category;
			std::function<std::unique_ptr<Component>()> create;
			int module = 0;
		};

		template <class T>
		static void Register(const char* category)
		{
			Add({ T::kTypeName, category, [] { return std::make_unique<T>(); } });
		}
		// 同じ名前がもうあれば置き換える（新しく読んだゲームの DLL の型が勝つ）
		static void Add(Entry entry);
		static std::unique_ptr<Component> Create(const std::string& name);
		static bool Has(const std::string& name);
		static const std::vector<Entry>& GetEntries();

		// これから Add されるものに付ける、モジュールの番号（GameModule が DLL を読む前後に設定する）
		static void SetLoadingModule(int module);
		// ゲームのモジュール（番号が 0 でないもの）の登録を外して返す
		static std::vector<Entry> RemoveGameEntries();
		// RemoveGameEntries で外したものを戻す（新しい DLL を読めなかったとき）
		static void RestoreEntries(std::vector<Entry> entries);
	};
}

#define RD_REGISTER_COMPONENT(Type, Category) \
	static const bool sRegistered_##Type = (::rd::ComponentRegistry::Register<Type>(Category), true)
