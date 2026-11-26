#pragma once
#include "Core/Serialization/Json.h"
#include <string>
#include <vector>

namespace rd
{
	class Actor;
	class Scene;

	// シーンとアクターの保存・読み込み（JSON）
	//  Scene は動いているアクターの入れ物で、ファイルの形は知らない。形はここだけが知っている。
	//  コンポーネントの値は Reflect で書き出し・読み込みするので、コンポーネントを足してもここは変えなくてよい
	//
	//  シーンのファイル: { "Version", "Name", "Environment", "Gravity", "Actors": [ アクター... ] }
	//  アクター: { "Id", "Parent", "Name", "Active", "Tag", "Layer", "Transform", "Components": [ { "Type", "Enabled", 値... } ] }
	//  型が見つからないコンポーネントは MissingComponent として値を持ったまま読み、保存し直しても値は消えない
	class RD_API SceneSerializer
	{
	public:
		static Json Serialize(const Scene& scene);
		// 今のアクターをすべて消してから読む（アクターの ID はファイルのまま）
		static bool Deserialize(Scene& scene, const Json& json);
		// 保存・読み込みできたら、シーンのパスをそのファイルにする
		static bool SaveToFile(Scene& scene, const std::string& path);
		static bool LoadFromFile(Scene& scene, const std::string& path);

		// アクター（と子）を JSON の配列へ
		static Json SerializeActors(const std::vector<Actor*>& actors);
		// JSON の配列からアクターを作る（remapIds なら新しい ID を振る）。作った中で親がいないものを返す
		//  一緒に作ったアクターを指す ObjectRef は新しい ID へ付け替え、それ以外は keepExternalRefs なら残す（複製）、
		//  そうでなければ外す（プレハブ）
		static std::vector<Actor*> DeserializeActors(Scene& scene, const Json& actors, bool remapIds, Actor* parent = nullptr, bool keepExternalRefs = true);

		// アクター自身の状態（名前・Transform・コンポーネント。子と親は含まない。Undo で使う）
		static Json SerializeActorState(const Actor* actor);
		// コンポーネントの並びが同じなら値だけ書き戻し、違えば作り直す
		static void ApplyActorState(Actor* actor, const Json& state);

		// シーンの設定（環境・重力）
		static Json SerializeSettings(const Scene& scene);
		static void ApplySettings(Scene& scene, const Json& json);

		// アクター（と子）をプレハブのファイルにする（ルートは親なしとして保存する）
		static bool SavePrefab(const Actor* actor, const std::string& path);
		// プレハブのファイルからアクターを作る（失敗したら nullptr）
		static Actor* InstantiatePrefab(Scene& scene, const std::string& path, Actor* parent);

	private:
		static void SerializeActor(const Actor* actor, Json& out);
		static bool ApplyComponentValues(Actor* actor, const Json& comps);
		static void RebuildComponents(Actor* actor, const Json& comps);
		static void AwakeComponents(Actor* actor);
	};
}
