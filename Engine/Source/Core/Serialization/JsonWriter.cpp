#include "RdPch.h"
#include "Core/Serialization/JsonWriter.h"

namespace rd
{
	bool JsonWriter::Property(const char* name, ObjectRefBase& value, const PropertyMeta&)
	{
		Top()[name] = Json{ { kRefKey, value.GetId() } };
		return false;
	}

	bool JsonWriter::BeginGroup(const char* name)
	{
		Json& child = Top()[name];
		child = Json::object();
		mStack.push_back(&child);
		return true;
	}

	bool JsonWriter::EnumProperty(const char* name, int64_t& value, std::span<const std::string_view> names,
		std::span<const int64_t> values, const PropertyMeta&)
	{
		for (size_t i = 0; i < values.size(); ++i)
		{
			if (values[i] == value)
			{
				Top()[name] = std::string(names[i]);
				return false;
			}
		}
		Top()[name] = value;
		return false;
	}
}
