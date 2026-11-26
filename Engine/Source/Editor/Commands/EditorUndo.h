#pragma once
#include "Core/Serialization/Json.h"
#include "Editor/Commands/CommandHistory.h"
#include "Scene/Actor/Actor.h"
#include <memory>
#include <string>

namespace rd
{
	class Editor;

	// エディタの Undo / Redo（CommandHistory に、操作の前後を持つコマンドを積む）
	//  プロパティやギズモの変更は、選んでいるアクターとシーンの設定の「変更前」を JSON で覚えておき、
	//  確定したとき（CommitActorEdit / CommitSettingsEdit）に「変更後」と比べてコマンドにする。
	//  プレイ中の変更は止めたときに元に戻るので、履歴に残さない
	class RD_API EditorUndo
	{
	public:
		explicit EditorUndo(Editor& editor) : mEditor(editor) {}

		// 選んでいるアクター / シーンの設定の変更を確定して積む（変わっていなければ何もしない）
		void CommitActorEdit(const std::string& label);
		void CommitSettingsEdit(const std::string& label);
		// 作った操作をそのまま積む（アクターの作成・削除・親の付け替えなど）
		void Push(std::unique_ptr<EditorCommand> command);
		void Undo();
		void Redo();

		// 選んでいるアクターとシーンの設定の「変更前」を取り直す（選び直した・シーンが変わったとき）
		void RefreshSnapshots();
		// 履歴を捨てる（別のシーンを開いたとき）
		void Clear() { mHistory.Clear(); }
		void MarkSaved() { mHistory.MarkSaved(); }
		bool IsDirty() const { return mHistory.IsDirty(); }
		const CommandHistory& GetHistory() const { return mHistory; }

	private:
		Editor& mEditor;
		CommandHistory mHistory;
		ActorId mSnapshotActor = 0;
		Json mActorSnapshot;
		Json mSettingsSnapshot;
	};
}
