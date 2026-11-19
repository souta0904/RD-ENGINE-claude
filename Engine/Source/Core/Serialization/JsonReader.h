#pragma once
#include "Core/Log/Log.h"
#include "Core/Reflection/PropertyVisitor.h"
#include "Core/Serialization/Json.h"

namespace rd
{
	// JSON から Reflect で並べたプロパティへ読み込む（JSON にあるキーだけ上書きする）
	//  JSON にないキー（後から足した変数など）は、コンストラクタで入れた既定値のまま残る。
	//  なので、古いシーンのファイルも新しいエンジンでそのまま読める
	//
	//  使い方:
	//    Json json;
	//    LoadJsonFile("x.json", json);
	//    JsonReader reader(json);
	//    component.Reflect(reader);
	class JsonReader final : public PropertyVisitor
	{
	public:
		// mStack … 今読んでいる JSON のオブジェクトの積み重ね。BeginGroup で中へ入り、EndGroup で外へ戻る
		explicit JsonReader(const Json& json) : mStack{ &json } {}

		// どの型も中身は同じ（Read に任せる）。Read が型ごとの from_json を使って変換する
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
		// 派生クラスで Property を 1 つでも書くと、基底の同じ名前の関数（enum 用のテンプレート）が隠れてしまう。
		// using で基底の Property もこのクラスから呼べるようにする
		using PropertyVisitor::Property;

		bool BeginGroup(const char* name) override;
		void EndGroup() override { mStack.pop_back(); }
		bool IsLoading() const override { return true; }

	protected:
		bool EnumProperty(const char* name, int64_t& value, std::span<const std::string_view> names,
			std::span<const int64_t> values, const PropertyMeta&) override;

	private:
		// 今のオブジェクトから name のキーを探して value に入れる
		//  キーがない・null なら何もしない（既定値のまま）
		//  型が合わない（数のところに文字列がある など）と get が例外を投げるので、警告を出して読み飛ばす。
		//  1 つの値が壊れていても、残りのプロパティは読める
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

		// グループが JSON にないときに積む空のオブジェクト（中のプロパティは何も見つからず、既定値のまま残る）
		static inline const Json kEmpty = Json::object();
		std::vector<const Json*> mStack;
	};
}
