#include "RdPch.h"
#include "Scene/ComponentRegistry.h"

namespace rd
{
	namespace
	{
		// 静的初期化順の問題を避けるため関数内 static
		std::vector<ComponentRegistry::Entry>& Entries()
		{
			static std::vector<ComponentRegistry::Entry> entries;
			return entries;
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
		std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b)
			{
				return a.category != b.category ? a.category < b.category : a.name < b.name;
			});
	}

	std::unique_ptr<Component> ComponentRegistry::Create(const std::string& name)
	{
		for (auto& e : Entries())
		{
			if (e.name == name) { return e.create(); }
		}
		Log::Error("Unknown component type: {}", name);
		return nullptr;
	}

	const std::vector<ComponentRegistry::Entry>& ComponentRegistry::GetEntries()
	{
		return Entries();
	}
}
