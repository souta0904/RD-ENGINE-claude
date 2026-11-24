#include "RdPch.h"
#include "Core/Serialization/JsonReader.h"

namespace rd
{
	// { "$ref": 12345 } の形なら、その数を ID として読む。形が違えば何もしない
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

	// グループの中へ入る。JSON にそのグループがなければ空のオブジェクトを積む
	// （必ず何かを積むので、対になる EndGroup の pop_back がずれない）
	// &*it … イテレータが指している JSON の値のアドレス
	bool JsonReader::BeginGroup(const char* name)
	{
		const Json& top = *mStack.back();
		auto it = top.find(name);
		mStack.push_back(it != top.end() && it->is_object() ? &*it : &kEmpty);
		return true;
	}

	// 名前（"Point" など）で保存されていれば、名前の一覧から探して値にする
	// 古いファイルで数のまま保存されているものも、そのまま読む
	// 知らない名前（enum の値を消した・名前を変えた）なら警告を出して、今の値のまま残す
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
			Logger::Warning(LogCat::Core, "JSON: unknown enum value '{}' for '{}'", str, name);
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
