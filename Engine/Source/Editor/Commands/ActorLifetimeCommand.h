#pragma once
#include "Core/Serialization/Json.h"
#include "Editor/Commands/EditorCommand.h"
#include <memory>
#include <vector>

namespace rd
{
	// アクター（と子）の作成と削除（作成と削除は向きが逆なだけなので 1 つのクラスにする）
	//  Created は作った直後、Deleting は消す直前に呼ぶ。中身は ID ごと JSON で覚えておく
	class RD_API ActorLifetimeCommand final : public EditorCommand
	{
	public:
		static std::unique_ptr<ActorLifetimeCommand> Created(std::string label, const std::vector<Actor*>& actors);
		static std::unique_ptr<ActorLifetimeCommand> Deleting(std::string label, const std::vector<Actor*>& actors);

		void Undo(Scene& scene) override { mCreated ? Remove(scene) : Restore(scene); }
		void Redo(Scene& scene) override { mCreated ? Restore(scene) : Remove(scene); }

	private:
		struct Entry
		{
			Json actors;
			ActorId root = 0;
			ActorId parent = 0;
			int siblingIndex = -1;
		};
		ActorLifetimeCommand(std::string label, bool created) : EditorCommand(std::move(label)), mCreated(created) {}
		void Capture(const std::vector<Actor*>& actors);
		void Restore(Scene& scene);
		void Remove(Scene& scene);

		bool mCreated;
		std::vector<Entry> mEntries;
	};
}
