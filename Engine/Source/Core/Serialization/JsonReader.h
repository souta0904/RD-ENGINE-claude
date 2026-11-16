#pragma once
#include "Core/Log/Log.h"
#include "Core/Reflection/PropertyVisitor.h"
#include "Core/Serialization/Json.h"

namespace rd
{
	// JSON から Reflect で並べたプロパティへ読み込む（JSON にあるキーだけ上書きする）
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
		// ID だけ読む（どのシーンの参照かは Scene が後で決める）
		bool Property(const char* name, ObjectRefBase& value, const PropertyMeta& = {}) override;
		using PropertyVisitor::Property;

		bool BeginGroup(const char* name) override;
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
				Log::Warning(LogCat::Core, "JSON: failed to read '{}': {}", name, e.what());
				return false;
			}
		}

		static inline const Json kEmpty = Json::object();
		std::vector<const Json*> mStack;
	};
}
