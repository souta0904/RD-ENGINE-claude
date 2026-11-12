#pragma once
#include "Scene/Component.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace rd
{
	// 型名からコンポーネントを生成するレジストリ
	//  エンジン組み込みのものは Engine 初期化時に登録される。
	//  ゲーム側は .cpp に RD_REGISTER_COMPONENT(MyComponent, "Game") と書けば自動で登録される。
	class ComponentRegistry
	{
	public:
		struct Entry
		{
			std::string name;
			std::string category;
			std::function<std::unique_ptr<Component>()> create;
		};

		template <class T>
		static void Register(const char* category)
		{
			Add({ T::kTypeName, category, [] { return std::make_unique<T>(); } });
		}
		static void Add(Entry entry);
		static std::unique_ptr<Component> Create(const std::string& name);
		static const std::vector<Entry>& GetEntries();
	};
}

#define RD_REGISTER_COMPONENT(Type, Category) \
	static const bool sRegistered_##Type = (::rd::ComponentRegistry::Register<Type>(Category), true)
