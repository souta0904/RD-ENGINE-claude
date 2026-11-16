#pragma once
#include "Core/Serialization/Json.h"
#include "Editor/Commands/EditorCommand.h"

namespace rd
{
	// シーンの設定（環境・重力）の変更
	class SceneSettingsCommand final : public EditorCommand
	{
	public:
		SceneSettingsCommand(std::string label, Json before, Json after)
			: EditorCommand(std::move(label)), mBefore(std::move(before)), mAfter(std::move(after)) {}
		void Undo(Scene& scene) override;
		void Redo(Scene& scene) override;

	private:
		Json mBefore;
		Json mAfter;
	};
}
