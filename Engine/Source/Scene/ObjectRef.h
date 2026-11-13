#pragma once
#include "Scene/Actor.h"

namespace rd
{
	class Scene;

	// シーン内のアクター（またはそのコンポーネント）への参照
	//  Unity のインスペクタの参照フィールドと同じように使える。
	//    - Reflect で v.Property("Target", target) と書くだけで、保存・Undo・インスペクタに対応する
	//    - エディタではヒエラルキーからドラッグ&ドロップ、または右の ◎ ボタンの一覧から設定できる
	//  中身は ActorId なので、相手が破棄されると Get() は nullptr を返す（ダングリングポインタにならない）。
	//  複製・プレハブ化したときは、コピーした範囲の中を指す参照はコピー先へ付け替えられる。
	class ObjectRef : public ObjectRefBase
	{
	public:
		// 参照先のアクター（未設定・破棄済みなら nullptr）
		Actor* GetActor() const;
		Scene* GetScene() const { return mScene; }
		// 参照先が今も生きているか
		explicit operator bool() const { return GetActor() != nullptr; }

		// 要求する型の名前（インスペクタの "None (Animator)" 表示に使う）
		const char* GetTypeName() const { return mTypeName; }
		// このアクターを代入できるか（ComponentRef なら目的のコンポーネントを持っているか）
		bool Accepts(const Actor* actor) const { return actor && (!mAccepts || mAccepts(actor)); }

		void SetActor(Actor* actor);
		// 読み込み後に所属シーンを決める（ID の読み書きは ObjectRefBase）
		void SetScene(Scene* scene) { mScene = scene; }

	protected:
		using AcceptFunc = bool(*)(const Actor*);
		ObjectRef(const char* typeName, AcceptFunc accepts) : mTypeName(typeName), mAccepts(accepts) {}

	private:
		Scene* mScene = nullptr;
		const char* mTypeName;
		AcceptFunc mAccepts;
	};

	// アクターへの参照
	class ActorRef final : public ObjectRef
	{
	public:
		ActorRef() : ObjectRef("Actor", nullptr) {}
		ActorRef(Actor* actor) : ActorRef() { SetActor(actor); }
		ActorRef& operator=(Actor* actor) { SetActor(actor); return *this; }

		Actor* Get() const { return GetActor(); }
		Actor* operator->() const { return GetActor(); }
	};

	// コンポーネントへの参照（参照先アクターの、最初の T を指す）
	template <class T>
	class ComponentRef final : public ObjectRef
	{
	public:
		ComponentRef() : ObjectRef(T::kTypeName, [](const Actor* a) { return a->GetComponent<T>() != nullptr; }) {}
		ComponentRef(T* component) : ComponentRef() { *this = component; }
		ComponentRef& operator=(T* component)
		{
			SetActor(component ? component->GetActor() : nullptr);
			return *this;
		}

		T* Get() const
		{
			Actor* actor = GetActor();
			return actor ? actor->GetComponent<T>() : nullptr;
		}
		T* operator->() const { return Get(); }
	};
}
