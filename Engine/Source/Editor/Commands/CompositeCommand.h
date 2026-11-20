#pragma once
#include "Editor/Commands/EditorCommand.h"
#include <memory>
#include <vector>

namespace rd
{
	// いくつかのコマンドを 1 回の操作としてまとめる（Undo は逆の順に戻す）
	class RD_API CompositeCommand final : public EditorCommand
	{
	public:
		// RD_API で DLL から出すと、コピーできないメンバーのコピーまで作ろうとするので禁止しておく
		CompositeCommand(const CompositeCommand&) = delete;
		CompositeCommand& operator=(const CompositeCommand&) = delete;
		using EditorCommand::EditorCommand;
		void Add(std::unique_ptr<EditorCommand> command) { mCommands.push_back(std::move(command)); }
		bool IsEmpty() const { return mCommands.empty(); }
		void Undo(Scene& scene) override;
		void Redo(Scene& scene) override;

	private:
		std::vector<std::unique_ptr<EditorCommand>> mCommands;
	};
}
