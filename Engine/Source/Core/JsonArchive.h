#pragma once
#include "Core/Log.h"
#include "Core/Reflection.h"
#include <nlohmann/json.hpp>

namespace rd
{
	using Json = nlohmann::ordered_json;

	// 数学型 <-> JSON 配列
	void to_json(Json& j, const Vector2& v);
	void to_json(Json& j, const Vector3& v);
	void to_json(Json& j, const Vector4& v);
	void to_json(Json& j, const Quaternion& v);
	void to_json(Json& j, const Color& v);
	void from_json(const Json& j, Vector2& v);
	void from_json(const Json& j, Vector3& v);
	void from_json(const Json& j, Vector4& v);
	void from_json(const Json& j, Quaternion& v);
	void from_json(const Json& j, Color& v);

	// Reflect の結果を JSON に書き出す
	class JsonWriter final : public PropertyVisitor
	{
	public:
		explicit JsonWriter(Json& json) : mStack{ &json } {}

		bool Property(const char* name, bool& value, const PropertyMeta& = {}) override { Top()[name] = value; return false; }
		bool Property(const char* name, int32_t& value, const PropertyMeta& = {}) override { Top()[name] = value; return false; }
		bool Property(const char* name, uint32_t& value, const PropertyMeta& = {}) override { Top()[name] = value; return false; }
		bool Property(const char* name, float& value, const PropertyMeta& = {}) override { Top()[name] = value; return false; }
		bool Property(const char* name, Vector2& value, const PropertyMeta& = {}) override { Top()[name] = value; return false; }
		bool Property(const char* name, Vector3& value, const PropertyMeta& = {}) override { Top()[name] = value; return false; }
		bool Property(const char* name, Vector4& value, const PropertyMeta& = {}) override { Top()[name] = value; return false; }
		bool Property(const char* name, Quaternion& value, const PropertyMeta& = {}) override { Top()[name] = value; return false; }
		bool Property(const char* name, Color& value, const PropertyMeta& = {}) override { Top()[name] = value; return false; }
		bool Property(const char* name, std::string& value, const PropertyMeta& = {}) override { Top()[name] = value; return false; }
		using PropertyVisitor::Property;

		bool BeginGroup(const char* name) override
		{
			Json& child = Top()[name];
			child = Json::object();
			mStack.push_back(&child);
			return true;
		}
		void EndGroup() override { mStack.pop_back(); }

	protected:
		bool EnumProperty(const char* name, int64_t& value, std::span<const std::string_view> names,
			std::span<const int64_t> values, const PropertyMeta&) override;

	private:
		Json& Top() { return *mStack.back(); }
		std::vector<Json*> mStack;
	};

	// JSON から Reflect の値へ読み込む（存在するキーだけ上書き）
	class JsonReader final : public PropertyVisitor
	{
	public:
		explicit JsonReader(const Json& json) : mStack{ &json } {}

		bool Property(const char* name, bool& value, const PropertyMeta& = {}) override { return Read(name, value); }
		bool Property(const char* name, int32_t& value, const PropertyMeta& = {}) override { return Read(name, value); }
		bool Property(const char* name, uint32_t& value, const PropertyMeta& = {}) override { return Read(name, value); }
		bool Property(const char* name, float& value, const PropertyMeta& = {}) override { return Read(name, value); }
		bool Property(const char* name, Vector2& value, const PropertyMeta& = {}) override { return Read(name, value); }
		bool Property(const char* name, Vector3& value, const PropertyMeta& = {}) override { return Read(name, value); }
		bool Property(const char* name, Vector4& value, const PropertyMeta& = {}) override { return Read(name, value); }
		bool Property(const char* name, Quaternion& value, const PropertyMeta& = {}) override { return Read(name, value); }
		bool Property(const char* name, Color& value, const PropertyMeta& = {}) override { return Read(name, value); }
		bool Property(const char* name, std::string& value, const PropertyMeta& = {}) override { return Read(name, value); }
		using PropertyVisitor::Property;

		bool BeginGroup(const char* name) override
		{
			const Json& top = *mStack.back();
			auto it = top.find(name);
			mStack.push_back(it != top.end() && it->is_object() ? &*it : &kEmpty);
			return true;
		}
		void EndGroup() override { mStack.pop_back(); }
		bool IsLoading() const override { return true; }

	protected:
		bool EnumProperty(const char* name, int64_t& value, std::span<const std::string_view> names,
			std::span<const int64_t> values, const PropertyMeta&) override;

	private:
		template <class T>
		bool Read(const char* name, T& value)
		{
			const Json& top = *mStack.back();
			auto it = top.find(name);
			if (it == top.end() || it->is_null()) { return false; }
			try
			{
				value = it->get<T>();
				return true;
			}
			catch (const std::exception& e)
			{
				Log::Warning("JSON: failed to read '{}': {}", name, e.what());
				return false;
			}
		}

		static inline const Json kEmpty = Json::object();
		std::vector<const Json*> mStack;
	};

	bool LoadJsonFile(const std::string& path, Json& out);
	bool SaveJsonFile(const std::string& path, const Json& json);
}
