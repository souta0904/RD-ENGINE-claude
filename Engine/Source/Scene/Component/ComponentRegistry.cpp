#include "RdPch.h"
#include "Scene/Component/ComponentRegistry.h"

namespace rd
{
	namespace
	{
		// 静的な初期化の順番に左右されないよう、関数の中の static にする
		std::vector<ComponentRegistry::Entry>& Entries()
		{
			static std::vector<ComponentRegistry::Entry> entries;
			return entries;
		}

		bool EntryOrder(const ComponentRegistry::Entry& a, const ComponentRegistry::Entry& b)
		{
			return a.category != b.category ? a.category < b.category : a.name < b.name;
		}
	}

	void ComponentRegistry::Add(Entry entry)
	{
		auto& entries = Entries();
		for (auto& e : entries)
		{
			if (e.name == entry.name)
			{
				e = std::move(entry);
				return;
			}
		}
		entries.push_back(std::move(entry));
		std::sort(entries.begin(), entries.end(), EntryOrder);
	}

	std::unique_ptr<Component> ComponentRegistry::Create(const std::string& name)
	{
		for (auto& e : Entries())
		{
			if (e.name == name) { return e.create(); }
		}
		Log::Error(LogCat::Scene, "Unknown component type: {}", name);
		return nullptr;
	}

	const std::vector<ComponentRegistry::Entry>& ComponentRegistry::GetEntries()
	{
		return Entries();
	}
}
