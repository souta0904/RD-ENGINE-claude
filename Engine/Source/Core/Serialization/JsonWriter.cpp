#include "RdPch.h"
#include "Core/Serialization/JsonWriter.h"

namespace rd
{
	// 参照は { "$ref": ID } の形で書く（Json.h の kRefKey）
	bool JsonWriter::Property(const char* name, ObjectRefBase& value, const PropertyMeta&)
	{
		Top()[name] = Json{ { kRefKey, value.GetId() } };
		return false;
	}

	// グループの名前で空のオブジェクトを作り、その中へ入る（以後の Property はこの中に書かれる）
	// Top()[name] はキーがなければ作って、その参照を返す
	bool JsonWriter::BeginGroup(const char* name)
	{
		Json& child = Top()[name];
		child = Json::object();
		mStack.push_back(&child);
		return true;
	}

	// 値に対応する名前を探して、名前で書く（enum の並びを変えても、古いファイルを読み違えない）
	// 一覧にない値（フラグを組み合わせた値など）は、名前がないので数のまま書く
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
