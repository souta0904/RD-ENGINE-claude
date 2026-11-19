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
	//
	//  仕組み（ビジターパターン）
	//    コンポーネントの側は、どの変数をどんな名前で見せるかだけを書く:
	//      void Light::Reflect(PropertyVisitor& v)
	//      {
	//          v.Property("Color", mColor);
	//          v.Property("Intensity", mIntensity, { .min = 0.0f, .max = 10.0f });
	//      }
	//    同じ Reflect に、別の種類の PropertyVisitor を渡すと、別の仕事になる:
	//      JsonWriter     … 値を JSON へ書く（保存）
	//      JsonReader     … JSON から値を読む（読み込み）
	//      ImGuiInspector … 値を画面に出して、編集できるようにする
	//    変数を 1 つ足すとき、Reflect に 1 行足すだけで、保存も表示も対応できる
	//
	//  C++ には型の中身を実行時に調べる仕組み（リフレクション）がないので、こうして自分で並べる
	class PropertyVisitor
	{
	public:
		virtual ~PropertyVisitor() = default;

		// 扱える型ごとの関数。派生クラスがそれぞれの型の読み書き・表示を実装する
		// value は参照で受け取るので、読み込みや編集ではその変数を直接書き換える
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
		//  仮想関数はテンプレートにできないので、どの enum も「整数 + 名前の一覧」という共通の形に直して渡す。
		//  magic_enum … enum の値の名前（"Point" など）をコンパイル時に取り出せるライブラリ
		//  static const … 一覧はその enum ごとに 1 回だけ作り、以後は使い回す
		//  requires std::is_enum_v<E> … enum の型のときだけ、この関数を候補にする
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
		//  BeginGroup が false を返したら（画面で閉じているなど）、中のプロパティを飛ばして EndGroup も呼ばない
		//  /*name*/ … 基底クラスでは使わない引数。名前をコメントにすると「使っていない」警告が出ない
		virtual bool BeginGroup(const char* /*name*/) { return true; }
		virtual void EndGroup() {}
		// 次のグループを、はじめは開いた状態で出す（画面の表示だけ。保存には関係しない）
		virtual void SetNextGroupOpen() {}

		// 読み込み中か（Reflect の中で「読み込んだ後に作り直す」などを分けたいときに使う）
		virtual bool IsLoading() const { return false; }

	protected:
		// 列挙型の実際の処理。names[i] と values[i] が対応している
		virtual bool EnumProperty(const char* name, int64_t& value,
			std::span<const std::string_view> names, std::span<const int64_t> values, const PropertyMeta& meta) = 0;

	private:
		// enum のすべての値を整数の配列にする
		template <class E>
		static std::vector<int64_t> EnumValues()
		{
			std::vector<int64_t> values;
			for (E e : magic_enum::enum_values<E>()) { values.push_back(int64_t(e)); }
			return values;
		}
	};
}
