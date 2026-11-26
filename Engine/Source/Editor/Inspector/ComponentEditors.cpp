#include "RdPch.h"
#include "Editor/Inspector/ComponentEditors.h"
#include <unordered_map>

namespace rd
{
	namespace
	{
		// 型 → 描く関数。関数の中の static なので、どこから最初に呼ばれても作られている
		std::unordered_map<std::type_index, ComponentEditors::DrawFunction>& GetTable()
		{
			static std::unordered_map<std::type_index, ComponentEditors::DrawFunction> table;
			return table;
		}
	}

	void ComponentEditors::RegisterType(std::type_index type, DrawFunction draw)
	{
		GetTable()[type] = std::move(draw);
	}

	bool ComponentEditors::Draw(Component& component)
	{
		auto& table = GetTable();
		auto it = table.find(std::type_index(typeid(component)));
		if (it == table.end()) { return false; }
		return it->second(component);
	}
}
