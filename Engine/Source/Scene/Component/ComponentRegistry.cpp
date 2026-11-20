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

		int sLoadingModule = 0;
	}

	void ComponentRegistry::Add(Entry entry)
	{
		entry.module = sLoadingModule;
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

	bool ComponentRegistry::Has(const std::string& name)
	{
		for (auto& e : Entries())
		{
			if (e.name == name) { return true; }
		}
		return false;
	}

	const std::vector<ComponentRegistry::Entry>& ComponentRegistry::GetEntries()
	{
		return Entries();
	}

	void ComponentRegistry::SetLoadingModule(int module)
	{
		sLoadingModule = module;
	}

	std::vector<ComponentRegistry::Entry> ComponentRegistry::RemoveGameEntries()
	{
		auto& entries = Entries();
		std::vector<Entry> removed;
		std::vector<Entry> kept;
		for (auto& e : entries)
		{
			if (e.module != 0) { removed.push_back(std::move(e)); }
			else { kept.push_back(std::move(e)); }
		}
		entries = std::move(kept);
		return removed;
	}

	void ComponentRegistry::RestoreEntries(std::vector<Entry> entries)
	{
		auto& all = Entries();
		for (auto& e : entries) { all.push_back(std::move(e)); }
		std::sort(all.begin(), all.end(), EntryOrder);
	}
}
