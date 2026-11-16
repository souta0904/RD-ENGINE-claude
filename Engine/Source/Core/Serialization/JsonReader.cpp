#include "RdPch.h"
#include "Core/Serialization/JsonReader.h"

namespace rd
{
	bool JsonReader::Property(const char* name, ObjectRefBase& value, const PropertyMeta&)
	{
		const Json& top = *mStack.back();
		auto it = top.find(name);
		if (it == top.end() || !it->is_object()) { return false; }
		auto ref = it->find(kRefKey);
		if (ref == it->end() || !ref->is_number_unsigned()) { return false; }
		value.SetId(ref->get<uint64_t>());
		return true;
	}

	bool JsonReader::BeginGroup(const char* name)
	{
		const Json& top = *mStack.back();
		auto it = top.find(name);
		mStack.push_back(it != top.end() && it->is_object() ? &*it : &kEmpty);
		return true;
	}

	bool JsonReader::EnumProperty(const char* name, int64_t& value, std::span<const std::string_view> names,
		std::span<const int64_t> values, const PropertyMeta&)
	{
		const Json& top = *mStack.back();
		auto it = top.find(name);
		if (it == top.end()) { return false; }
		if (it->is_string())
		{
			auto str = it->get<std::string>();
			for (size_t i = 0; i < names.size(); ++i)
			{
				if (names[i] == str)
				{
					value = values[i];
					return true;
				}
			}
			Log::Warning(LogCat::Core, "JSON: unknown enum value '{}' for '{}'", str, name);
			return false;
		}
		if (it->is_number_integer())
		{
			value = it->get<int64_t>();
			return true;
		}
		return false;
	}
}
