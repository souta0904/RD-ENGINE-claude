#include "RdPch.h"
#include "Core/JsonArchive.h"
#include "Core/StringUtil.h"

namespace rd
{
	void to_json(Json& j, const Vector2& v) { j = Json::array({ v.x, v.y }); }
	void to_json(Json& j, const Vector3& v) { j = Json::array({ v.x, v.y, v.z }); }
	void to_json(Json& j, const Vector4& v) { j = Json::array({ v.x, v.y, v.z, v.w }); }
	void to_json(Json& j, const Quaternion& v) { j = Json::array({ v.w, v.x, v.y, v.z }); }
	void to_json(Json& j, const Color& v) { j = Json::array({ v.r, v.g, v.b, v.a }); }

	namespace
	{
		void ReadFloats(const Json& j, float* out, size_t count)
		{
			if (!j.is_array()) { throw std::runtime_error("expected array"); }
			for (size_t i = 0; i < count && i < j.size(); ++i) { out[i] = j[i].get<float>(); }
		}
	}

	void from_json(const Json& j, Vector2& v) { ReadFloats(j, &v.x, 2); }
	void from_json(const Json& j, Vector3& v) { ReadFloats(j, &v.x, 3); }
	void from_json(const Json& j, Vector4& v) { ReadFloats(j, &v.x, 4); }
	void from_json(const Json& j, Quaternion& v) { ReadFloats(j, &v.w, 4); }
	void from_json(const Json& j, Color& v) { ReadFloats(j, &v.r, 4); }

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

	// 参照は { "$ref": ActorId } として保存する（0 は未設定）
	bool JsonWriter::Property(const char* name, ObjectRefBase& value, const PropertyMeta&)
	{
		Top()[name] = Json{ { kRefKey, value.GetId() } };
		return false;
	}

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

	bool IsRefJson(const Json& j)
	{
		return j.is_object() && j.size() == 1 && j.contains(kRefKey);
	}

	bool LoadJsonFile(const std::string& path, Json& out)
	{
		std::string text;
		if (!ReadTextFile(path, text)) { return false; }
		try
		{
			out = Json::parse(text);
			return true;
		}
		catch (const std::exception& e)
		{
			Log::Error(LogCat::Core, "Failed to parse JSON '{}': {}", path, e.what());
			return false;
		}
	}

	bool SaveJsonFile(const std::string& path, const Json& json)
	{
		return WriteTextFile(path, json.dump(4));
	}
}
