#pragma once
#include "Core/JsonArchive.h"
#include "Scene/Actor.h"
#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace rd
{
	class Scene;

	// エディタの操作 1 回分（コマンドパターン）
	//  操作した「後」に作って CommandHistory へ積む。Undo / Redo はシーンを受け取って元に戻す / やり直す。
	//  アクターはポインタではなく ID で覚える（Undo で作り直されても同じ ID になる）。
	class EditorCommand
	{
	public:
		explicit EditorCommand(std::string label) : mLabel(std::move(label)) {}
		virtual ~EditorCommand() = default;

		virtual void Undo(Scene& scene) = 0;
		virtual void Redo(Scene& scene) = 0;
		// 直前のコマンドと 1 つにまとめられるなら取り込んで true（連続したキー入力など）
		virtual bool MergeWith(const EditorCommand& /*next*/) { return false; }

		const std::string& GetLabel() const { return mLabel; }
		// Undo / Redo の後に選択するアクター
		ActorId GetSelection() const { return mSelection; }
		void SetSelection(ActorId id) { mSelection = id; }

	protected:
		std::string mLabel;
		ActorId mSelection = 0;
	};

	// アクター 1 つの状態の変更（名前・Transform・コンポーネントの値・追加・削除・並び替え）
	class ActorStateCommand final : public EditorCommand
	{
	public:
		ActorStateCommand(std::string label, ActorId actor, Json before, Json after);
		void Undo(Scene& scene) override;
		void Redo(Scene& scene) override;
		// 同じアクターへの同じ種類の操作が短い間隔で続いたら 1 つにする
		bool MergeWith(const EditorCommand& next) override;

	private:
		ActorId mActor;
		Json mBefore;
		Json mAfter;
		std::chrono::steady_clock::time_point mTime = std::chrono::steady_clock::now();
	};

	// アクター（と子）の作成と削除。Create と Delete は向きが逆なだけなので 1 つのクラスにする
	class ActorLifetimeCommand final : public EditorCommand
	{
	public:
		// 作成した直後に呼ぶ
		static std::unique_ptr<ActorLifetimeCommand> Created(std::string label, Scene& scene, const std::vector<Actor*>& actors);
		// 削除する直前に呼ぶ
		static std::unique_ptr<ActorLifetimeCommand> Deleting(std::string label, Scene& scene, const std::vector<Actor*>& actors);

		void Undo(Scene& scene) override { mCreated ? Remove(scene) : Restore(scene); }
		void Redo(Scene& scene) override { mCreated ? Restore(scene) : Remove(scene); }

	private:
		struct Entry
		{
			Json actors;// ルートとその子（ID はそのまま）
			ActorId root = 0;
			ActorId parent = 0;
			int siblingIndex = -1;
		};
		ActorLifetimeCommand(std::string label, bool created) : EditorCommand(std::move(label)), mCreated(created) {}
		void Capture(Scene& scene, const std::vector<Actor*>& actors);
		void Restore(Scene& scene);
		void Remove(Scene& scene);

		bool mCreated;
		std::vector<Entry> mEntries;
	};

	// 親子の付け替え（ワールド上の位置は保つ）
	class ReparentCommand final : public EditorCommand
	{
	public:
		// 付け替える直前に呼ぶ
		ReparentCommand(std::string label, Actor* actor, Actor* newParent, int newIndex = -1);
		void Undo(Scene& scene) override { Apply(scene, mOldParent, mOldIndex); }
		void Redo(Scene& scene) override { Apply(scene, mNewParent, mNewIndex); }

	private:
		void Apply(Scene& scene, ActorId parent, int index);
		ActorId mActor;
		ActorId mOldParent;
		int mOldIndex;
		ActorId mNewParent;
		int mNewIndex;
	};

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

	// 複数のコマンドを 1 回の操作としてまとめる
	class CompositeCommand final : public EditorCommand
	{
	public:
		using EditorCommand::EditorCommand;
		void Add(std::unique_ptr<EditorCommand> command) { mCommands.push_back(std::move(command)); }
		bool IsEmpty() const { return mCommands.empty(); }
		void Undo(Scene& scene) override;
		void Redo(Scene& scene) override;

	private:
		std::vector<std::unique_ptr<EditorCommand>> mCommands;
	};
}
