#pragma once
#include "Core/Reflection/ObjectRefBase.h"
#include "Core/Reflection/PropertyMeta.h"
#include "Math/Color.h"
#include "Math/Quaternion.h"
#include "Math/Vector4.h"
#include <magic_enum/magic_enum.hpp>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace rd
{
	// プロパティを走査するビジター
	//  コンポーネントは Reflect(PropertyVisitor&) に自分のプロパティを並べるだけで、
	//  JSON の保存/読み込み・インスペクタ表示・Undo 用の状態の保存のすべてに対応できる。
	//  各関数は「値が変わった（読み込まれた）か」を返す。
	class PropertyVisitor
	{
	public:
		virtual ~PropertyVisitor() = default;

		virtual bool Property(const char* name, bool& value, const PropertyMeta& meta = {}) = 0;
		virtual bool Property(const char* name, int32_t& value, const PropertyMeta& meta = {}) = 0;
		virtual bool Property(const char* name, uint32_t& value, const PropertyMeta& meta = {}) = 0;
		virtual bool Property(const char* name, float& value, const PropertyMeta& meta = {}) = 0;
		virtual bool Property(const char* name, Vector2& value, const PropertyMeta& meta = {}) = 0;
		virtual bool Property(const char* name, Vector3& value, const PropertyMeta& meta = {}) = 0;
		virtual bool Property(const char* name, Vector4& value, const PropertyMeta& meta = {}) = 0;
		virtual bool Property(const char* name, Quaternion& value, const PropertyMeta& meta = {}) = 0;
		virtual bool Property(const char* name, Color& value, const PropertyMeta& meta = {}) = 0;
		virtual bool Property(const char* name, std::string& value, const PropertyMeta& meta = {}) = 0;
		virtual bool Property(const char* name, ObjectRefBase& value, const PropertyMeta& meta = {}) = 0;

		// 列挙型は名前の一覧と値の一覧に変換して EnumProperty へ渡す（名前で保存するので並び替えに強い）
		template <class E> requires std::is_enum_v<E>
		bool Property(const char* name, E& value, const PropertyMeta& meta = {})
		{
			static const std::vector<std::string_view> kNames(magic_enum::enum_names<E>().begin(), magic_enum::enum_names<E>().end());
			static const std::vector<int64_t> kValues = EnumValues<E>();
			int64_t raw = int64_t(value);
			bool changed = EnumProperty(name, raw, kNames, kValues, meta);
			value = E(raw);
			return changed;
		}

		// 折りたたみのグループ（JSON ではネストしたオブジェクトになる）
		virtual bool BeginGroup(const char* /*name*/) { return true; }
		virtual void EndGroup() {}

		virtual bool IsLoading() const { return false; }

	protected:
		virtual bool EnumProperty(const char* name, int64_t& value,
			std::span<const std::string_view> names, std::span<const int64_t> values, const PropertyMeta& meta) = 0;

	private:
		template <class E>
		static std::vector<int64_t> EnumValues()
		{
			std::vector<int64_t> values;
			for (E e : magic_enum::enum_values<E>()) { values.push_back(int64_t(e)); }
			return values;
		}
	};
}
