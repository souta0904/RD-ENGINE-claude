#pragma once
#include "Core/Reflection/ObjectRefBase.h"

namespace rd
{
	class Actor;
	class Scene;

	// シーンの中のアクター（またはそのコンポーネント）への参照の基底
	//  Unity のインスペクタの参照の欄と同じように使える（Reflect で v.Property("Target", target) と書くだけで
	//  保存・Undo・インスペクタに対応し、エディタではヒエラルキーからのドラッグか ◎ ボタンの一覧から設定できる）。
	//  中身は ActorId なので、相手が消えると nullptr を返す（消えたものを指したままにならない）。
	//  複製やプレハブにしたときは、コピーした範囲の中を指す参照はコピー先へ付け替えられる。
	class RD_API ObjectRef : public ObjectRefBase
	{
	public:
		Actor* GetActor() const;
		Scene* GetScene() const { return mScene; }
		explicit operator bool() const { return GetActor() != nullptr; }

		// 求める型の名前（インスペクタの "None (Animator)" の表示に使う）
		const char* GetTypeName() const { return mTypeName; }
		// このアクターを入れられるか（ComponentRef なら目的のコンポーネントを持っているか）
		bool Accepts(const Actor* actor) const { return actor && (!mAccepts || mAccepts(actor)); }

		void SetActor(Actor* actor);
		// 読み込んだ後に、どのシーンの参照かを決める
		void SetScene(Scene* scene) { mScene = scene; }

	protected:
		using AcceptFunc = bool(*)(const Actor*);
		ObjectRef(const char* typeName, AcceptFunc accepts) : mTypeName(typeName), mAccepts(accepts) {}

	private:
		Scene* mScene = nullptr;
		const char* mTypeName;
		AcceptFunc mAccepts;
	};
}
