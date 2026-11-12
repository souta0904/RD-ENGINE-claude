#pragma once
#include "Core/JsonArchive.h"
#include "Render/RenderTypes.h"
#include "Scene/Actor.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace rd
{
	class CameraComponent;
	class DebugDraw;
	class PhysicsWorld;

	// アクターの集合（レベル）
	class Scene
	{
	public:
		Scene();
		~Scene();

		const std::string& GetName() const { return mName; }
		void SetName(std::string name) { mName = std::move(name); }
		const std::string& GetPath() const { return mPath; }
		void SetPath(std::string path) { mPath = std::move(path); }

		// アクター
		Actor* CreateActor(const std::string& name = "Actor", Actor* parent = nullptr);
		Actor* FindActor(ActorId id) const;
		Actor* FindActorByName(const std::string& name) const;
		std::vector<Actor*> FindActorsWithTag(const std::string& tag) const;
		const std::vector<Actor*>& GetRootActors() const { return mRootActors; }
		size_t GetActorCount() const { return mActors.size(); }
		// 全アクターを階層順に走査
		template <class Func>
		void ForEachActor(Func&& func) const
		{
			for (Actor* root : mRootActors) { VisitActor(root, func); }
		}
		// 指定型のコンポーネントをすべて集める
		template <class T>
		std::vector<T*> FindComponents() const
		{
			std::vector<T*> result;
			for (auto& a : mActors)
			{
				for (auto& c : a->GetAllComponents())
				{
					if (auto p = dynamic_cast<T*>(c.get()); p && p->IsActiveAndEnabled()) { result.push_back(p); }
				}
			}
			return result;
		}

		// 更新（playing=false なら編集中の更新だけ行う）
		void Update(float dt, bool playing);
		// 破棄予約されたアクター/コンポーネントをすぐに破棄する
		void FlushPendingDestroy() { if (mHasPendingDestroy) { FlushDestroyed(); } }
		void CollectRender(RenderList& list);
		void DrawGizmos(DebugDraw& draw, const Actor* selected);
		CameraComponent* GetMainCamera() const;

		// シリアライズ
		Json Serialize() const;
		bool Deserialize(const Json& json);
		bool SaveToFile(const std::string& path);
		bool LoadFromFile(const std::string& path);
		// アクター（と子）を JSON 配列へ
		Json SerializeActors(const std::vector<Actor*>& actors) const;
		// JSON 配列からアクターを生成（remapIds=true なら新しい ID を振る）
		std::vector<Actor*> DeserializeActors(const Json& actors, bool remapIds, Actor* parent = nullptr);
		// プレハブ
		Actor* Instantiate(const std::string& prefabPath, Actor* parent = nullptr);
		Actor* Duplicate(Actor* actor);
		static bool SavePrefab(const Actor* actor, const std::string& path);

		Environment& GetEnvironment() { return mEnvironment; }
		PhysicsWorld& GetPhysics() { return *mPhysics; }
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

		std::string mName = "Untitled";
		std::string mPath;
		std::vector<std::unique_ptr<Actor>> mActors;
		std::vector<Actor*> mRootActors;
		std::unordered_map<ActorId, Actor*> mIdMap;
		ActorId mNextId = 1;
		bool mHasPendingDestroy = false;
		bool mPlaying = false;
		float mFixedAccumulator = 0.0f;
		Environment mEnvironment;
		std::unique_ptr<PhysicsWorld> mPhysics;
	};
}
