#pragma once
#include "Core/Serialization/Json.h"
#include "Render/RenderList/RenderList.h"
#include "Render/Settings/Environment.h"
#include "Scene/Actor/Actor.h"
#include "Scene/SceneSystem.h"
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace rd
{
	class DebugDraw;

	// アクターの集まり（レベル）
	//  コンポーネントは具体的な型ごとのリストと、上書きしたコールバックごとのリストで持つ
	//  （全アクターを調べずに型で探せ、Update のないコンポーネントは呼ばない）
	//  実装は Scene.cpp（アクター・更新・登録）と SceneSerialization.cpp（保存・読み込み・プレハブ）
	class RD_API Scene
	{
	public:
		// RD_API で DLL から出すと、コピーできないメンバーのコピーまで作ろうとするので禁止しておく
		Scene(const Scene&) = delete;
		Scene& operator=(const Scene&) = delete;
		Scene();
		~Scene();

		const std::string& GetName() const { return mName; }
		void SetName(std::string name) { mName = std::move(name); }
		const std::string& GetPath() const { return mPath; }
		void SetPath(std::string path) { mPath = std::move(path); }

		Actor* CreateActor(const std::string& name = "Actor", Actor* parent = nullptr);
		Actor* FindActor(ActorId id) const;
		Actor* FindActorByName(const std::string& name) const;
		std::vector<Actor*> FindActorsWithTag(const std::string& tag) const;
		const std::vector<Actor*>& GetRootActors() const { return mRootActors; }
		size_t GetActorCount() const { return mActors.size(); }
		// すべてのアクターを階層の順に辿る
		template <class Func>
		void ForEachActor(Func&& func) const
		{
			for (Actor* root : mRootActors) { VisitActor(root, func); }
		}
		// T（派生型を含む）の有効なコンポーネントをすべて集める
		template <class T>
		std::vector<T*> FindComponents() const
		{
			std::vector<T*> result;
			ForEachComponent<T>([&](T* c) { result.push_back(c); });
			return result;
		}
		template <class T>
		T* FindComponent() const
		{
			T* found = nullptr;
			ForEachComponent<T>([&](T* c) { if (!found) { found = c; } });
			return found;
		}
		// 同じリストの中身はすべて同じ型なので、先頭だけ調べて型が合うリストを丸ごと渡す
		template <class T, class Func>
		void ForEachComponent(Func&& func) const
		{
			for (const TypeBucket& bucket : mBuckets)
			{
				if (bucket.components.empty() || !dynamic_cast<T*>(bucket.components.front())) { continue; }
				for (Component* c : bucket.components)
				{
					if (c->IsActiveAndEnabled()) { func(static_cast<T*>(c)); }
				}
			}
		}

		// playing=false なら編集中の更新だけ行う
		void Update(float dt, bool playing);
		void FlushPendingDestroy() { if (mHasPendingDestroy) { FlushDestroyed(); } }
		void CollectRender(RenderList& list);
#if RD_EDITOR
		void DrawGizmos(DebugDraw& draw, const Actor* selected);
#endif

		// 初めて呼んだときに作られる
		template <class T> requires std::derived_from<T, SceneSystem>
		T& GetSystem()
		{
			auto it = mSystemMap.find(typeid(T));
			if (it != mSystemMap.end()) { return static_cast<T&>(*it->second); }
			auto system = std::make_unique<T>();
			T& ref = *system;
			mSystemMap.emplace(typeid(T), system.get());
			mSystems.push_back(std::move(system));
			return ref;
		}
		template <class T> requires std::derived_from<T, SceneSystem>
		T* FindSystem() const
		{
			auto it = mSystemMap.find(typeid(T));
			return it != mSystemMap.end() ? static_cast<T*>(it->second) : nullptr;
		}

		Json Serialize() const;
		bool Deserialize(const Json& json);
		bool SaveToFile(const std::string& path);
		bool LoadFromFile(const std::string& path);
		// アクター（と子）を JSON の配列へ
		Json SerializeActors(const std::vector<Actor*>& actors) const;
		// JSON の配列からアクターを作る（remapIds なら新しい ID を振る）
		//  一緒に作ったアクターを指す ObjectRef は新しい ID へ付け替え、それ以外は keepExternalRefs なら残す（複製）、
		//  そうでなければ外す（プレハブ）
		std::vector<Actor*> DeserializeActors(const Json& actors, bool remapIds, Actor* parent = nullptr, bool keepExternalRefs = true);
		// アクター自身の状態（名前・Transform・コンポーネント。子と親は含まない。Undo で使う）
		Json SerializeActorState(const Actor* actor) const;
		void ApplyActorState(Actor* actor, const Json& state);
		// シーンの設定（環境・重力）
		Json SerializeSettings() const;
		void ApplySettings(const Json& json);

		Actor* Instantiate(const std::string& prefabPath, Actor* parent = nullptr);
		Actor* Duplicate(Actor* actor);
		static bool SavePrefab(const Actor* actor, const std::string& path);

		Environment& GetEnvironment() { return mEnvironment; }
		// 重力（物理とパーティクルが使う。シーンに保存される）
		const Vector3& GetGravity() const { return mGravity; }
		void SetGravity(const Vector3& gravity) { mGravity = gravity; }
		bool IsPlaying() const { return mPlaying; }

	private:
		friend class Actor;
		template <class Func>
		static void VisitActor(Actor* a, Func& func)
		{
			func(a);
			for (Actor* child : a->GetChildren()) { VisitActor(child, func); }
		}
		void SerializeActor(const Actor* actor, Json& out) const;
		void FlushDestroyed();
		void Clear();
		bool ApplyComponentValues(Actor* actor, const Json& comps);
		void RebuildComponents(Actor* actor, const Json& comps);
		void AwakeComponents(Actor* actor);

		void RegisterComponent(Component* component);
		void UnregisterComponents(const std::vector<Component*>& components);
		void StartPendingComponents();

		// components は追加された順
		struct TypeBucket
		{
			std::type_index type;
			std::vector<Component*> components;
		};
		// コールバックごとの呼び出しの一覧（上書きしているコンポーネントだけ、追加された順）
		struct CallbackLists
		{
			std::vector<Component*> update;
			std::vector<Component*> fixedUpdate;
			std::vector<Component*> lateUpdate;
			std::vector<Component*> render;
			std::vector<Component*> editorUpdate;
			std::vector<Component*> gizmos;
		};

		std::string mName = "Untitled";
		std::string mPath;
		std::vector<std::unique_ptr<Actor>> mActors;
		std::vector<Actor*> mRootActors;
		std::unordered_map<ActorId, Actor*> mIdMap;
		ActorId mNextId = 1;
		bool mHasPendingDestroy = false;
		bool mPlaying = false;
		float mFixedAccumulator = 0.0f;
		// 型が最初に登録された順（並びを毎回同じにするため）
		std::vector<TypeBucket> mBuckets;
		std::unordered_map<std::type_index, size_t> mBucketIndex;
		CallbackLists mCallbacks;
		std::vector<Component*> mPendingStart;
		Environment mEnvironment;
		Vector3 mGravity = { 0.0f, -9.81f, 0.0f };
		// システムはアクターより後に消す（アクターの OnDestroy からシステムを使うため）
		std::vector<std::unique_ptr<SceneSystem>> mSystems;
		std::unordered_map<std::type_index, SceneSystem*> mSystemMap;
	};
}
