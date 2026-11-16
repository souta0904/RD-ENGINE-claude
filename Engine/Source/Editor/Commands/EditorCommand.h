#pragma once
#include "Scene/Actor/Actor.h"
#include <string>

namespace rd
{
	class Scene;

	// エディタの操作 1 回分（コマンドパターン）
	//  操作した「後」に作って CommandHistory へ積む。Undo / Redo はシーンを受け取って元に戻す / やり直す。
	//  アクターはポインタではなく ID で覚える（Undo で作り直されても同じ ID になる）。
	//  GetSelection は Undo / Redo の後に選ぶアクター
	class EditorCommand
	{
	public:
		explicit EditorCommand(std::string label) : mLabel(std::move(label)) {}
		virtual ~EditorCommand() = default;

		virtual void Undo(Scene& scene) = 0;
		virtual void Redo(Scene& scene) = 0;
		// 直前のコマンドと 1 つにまとめられるなら取り込んで true（続けて打ったキーなど）
		virtual bool MergeWith(const EditorCommand& /*next*/) { return false; }

		const std::string& GetLabel() const { return mLabel; }
		ActorId GetSelection() const { return mSelection; }
		void SetSelection(ActorId id) { mSelection = id; }

	protected:
		std::string mLabel;
		ActorId mSelection = 0;
	};
}
