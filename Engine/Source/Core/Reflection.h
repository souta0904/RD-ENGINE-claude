#pragma once
#include "Math/Math.h"
#include <magic_enum/magic_enum.hpp>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace rd
{
	// 参照プロパティの基底（中身は参照先の ID だけ）
	//  Core はシーンを知らないので、ここでは ID の読み書きだけを扱う。
	//  実際に使うのは Scene/ObjectRef.h の ActorRef / ComponentRef<T>。
	class ObjectRefBase
	{
	public:
		uint64_t GetId() const { return mId; }
		void SetId(uint64_t id) { mId = id; }
		void Clear() { mId = 0; }

	protected:
		ObjectRefBase() = default;
		uint64_t mId = 0;
	};

	// プロパティの付加情報（エディタ表示用）
	struct PropertyMeta
	{
		enum Flags : uint32_t
		{
			None = 0,
			Angle = 1 << 0,// ラジアン値を度で表示
			HDR = 1 << 1,// 1 を超える色を許可
			ReadOnly = 1 << 2,
			AssetTexture = 1 << 3,// アセットパス（ドラッグ&ドロップ対応）
			AssetModel = 1 << 4,
			AssetAudio = 1 << 5,
			AssetPrefab = 1 << 6,
			AssetScene = 1 << 7,
			Multiline = 1 << 8,
		};
		float speed = 0.05f;
		float min = 0.0f;
		float max = 0.0f;// min == max なら無制限
		uint32_t flags = None;
		const char* tooltip = nullptr;
	};

	// プロパティを走査するビジター
	//  コンポーネントは Reflect(PropertyVisitor&) に自分のプロパティを列挙するだけで、
	//  JSON 保存/読み込み・インスペクタ表示・Undo 用スナップショットのすべてに対応できる。
	//  各関数は「値が変更された（読み込まれた）か」を返す。
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
		// アクター/コンポーネントへの参照（ActorRef / ComponentRef<T>。Scene/ObjectRef.h）
		virtual bool Property(const char* name, ObjectRefBase& value, const PropertyMeta& meta = {}) = 0;

		// 列挙型（名前で保存されるので並び替えに強い）
		template <class E> requires std::is_enum_v<E>
		bool Property(const char* name, E& value, const PropertyMeta& meta = {})
		{
			static const auto kNames = [] {
				std::vector<std::string_view> v;
				for (auto n : magic_enum::enum_names<E>()) { v.push_back(n); }
				return v;
			}();
			static const auto kValues = [] {
				std::vector<int64_t> v;
				for (auto e : magic_enum::enum_values<E>()) { v.push_back(int64_t(e)); }
				return v;
			}();
			int64_t raw = int64_t(value);
			bool changed = EnumProperty(name, raw, kNames, kValues, meta);
			value = E(raw);
			return changed;
		}

		// 折りたたみグループ（JSON ではネストしたオブジェクトになる）
		virtual bool BeginGroup(const char* /*name*/) { return true; }
		virtual void EndGroup() {}

		// JSON から読み込み中か（読み込み後の再構築判定に使う）
		virtual bool IsLoading() const { return false; }

	protected:
		virtual bool EnumProperty(const char* name, int64_t& value,
			std::span<const std::string_view> names, std::span<const int64_t> values, const PropertyMeta& meta) = 0;
	};
}
