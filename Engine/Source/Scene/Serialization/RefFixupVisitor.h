#pragma once
#include "Core/Reflection/PropertyVisitor.h"
#include "Scene/Actor/Actor.h"
#include <unordered_map>

namespace rd
{
	// 読み込んだ ObjectRef をシーンに結び付け、ID を付け替えるビジター（参照以外は何もしない）
	//  remap なら、一緒に作ったアクター（idMap にある古い ID）を指す参照を新しい ID へ付け替える。
	//  それ以外を指す参照は keepExternal なら残し（複製）、そうでなければ外す（プレハブ）。
	class RD_API RefFixupVisitor final : public PropertyVisitor
	{
	public:
		RefFixupVisitor(Scene* scene, const std::unordered_map<ActorId, Actor*>& idMap, bool remap, bool keepExternal)
			: mScene(scene), mIdMap(idMap), mRemap(remap), mKeepExternal(keepExternal) {}

		bool Property(const char*, ObjectRefBase& base, const PropertyMeta&) override;

		bool Property(const char*, bool&, const PropertyMeta&) override { return false; }
		bool Property(const char*, int32_t&, const PropertyMeta&) override { return false; }
		bool Property(const char*, uint32_t&, const PropertyMeta&) override { return false; }
		bool Property(const char*, float&, const PropertyMeta&) override { return false; }
		bool Property(const char*, Vector2&, const PropertyMeta&) override { return false; }
		bool Property(const char*, Vector3&, const PropertyMeta&) override { return false; }
		bool Property(const char*, Vector4&, const PropertyMeta&) override { return false; }
		bool Property(const char*, Quaternion&, const PropertyMeta&) override { return false; }
		bool Property(const char*, Color&, const PropertyMeta&) override { return false; }
		bool Property(const char*, std::string&, const PropertyMeta&) override { return false; }
		using PropertyVisitor::Property;

	protected:
		bool EnumProperty(const char*, int64_t&, std::span<const std::string_view>, std::span<const int64_t>, const PropertyMeta&) override { return false; }

	private:
		Scene* mScene;
		const std::unordered_map<ActorId, Actor*>& mIdMap;
		bool mRemap;
		bool mKeepExternal;
	};
}
