#pragma once
#include "Core/Reflection/PropertyVisitor.h"
#include "Core/Serialization/Json.h"

namespace rd
{
	// Reflect で並べたプロパティを JSON へ書き出す
	//
	//  使い方:
	//    Json json;
	//    JsonWriter writer(json);
	//    component.Reflect(writer);
	//    SaveJsonFile("x.json", json);
	//
	//  書くだけで値は変えないので、どの Property も false（変わっていない）を返す
	class RD_API JsonWriter final : public PropertyVisitor
	{
	public:
		// mStack … 今書いている JSON のオブジェクトの積み重ね。BeginGroup で中へ入り、EndGroup で外へ戻る
		explicit JsonWriter(Json& json) : mStack{ &json } {}

		// Top()[name] = value … 今のオブジェクトに name のキーを作って値を入れる
		// 数学型は Json.h の to_json で配列になる
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
		// 基底の enum 用のテンプレートが隠れないようにする（JsonReader.h と同じ理由）
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
