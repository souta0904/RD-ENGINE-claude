#pragma once
#include "Scene/Actor/Actor.h"

namespace rd
{
	class Editor;

	// エディタで選んでいるアクター（1 つ）
	//  アクターのポインタではなく ID で覚えるので、Undo や読み直しでアクターが作り直されても選んだままになる
	class RD_API EditorSelection
	{
	public:
		explicit EditorSelection(Editor& editor) : mEditor(editor) {}

		// 選んでいるアクター（選んでいない・消えたときは nullptr）
		Actor* Get() const;
		ActorId GetId() const { return mId; }

		// 選び直す。プロジェクトパネルの選択を外し、Undo の「変更前」を取り直す
		void Select(Actor* actor);
		// ID だけを置き換える（Undo / Redo・シーンの差し替えで使う。ほかには何もしない）
		void SetId(ActorId id) { mId = id; }

	private:
		Editor& mEditor;
		ActorId mId = 0;
	};
}
