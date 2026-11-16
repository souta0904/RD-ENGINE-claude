#pragma once
#include "Core/Reflection/PropertyVisitor.h"
#include "Core/Serialization/Json.h"

namespace rd
{
	// Reflect で並べたプロパティを JSON へ書き出す
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
		bool Property(const char* name, ObjectRefBase& value, const PropertyMeta& = {}) override;
		using PropertyVisitor::Property;

		bool BeginGroup(const char* name) override;
		void EndGroup() override { mStack.pop_back(); }

	protected:
		bool EnumProperty(const char* name, int64_t& value, std::span<const std::string_view> names,
			std::span<const int64_t> values, const PropertyMeta&) override;

	private:
		Json& Top() { return *mStack.back(); }
		std::vector<Json*> mStack;
	};
}
