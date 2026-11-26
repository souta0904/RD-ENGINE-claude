#pragma once
#include "AI/BehaviorTree/BehaviorTreeRegistry.h"
#include "Math/Vector2.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace rd
{
	class Actor;

	namespace bt
	{
		// アセットの中のノード 1 つ
		//  children は出口ごとの子の番号（0 は「つないでいない」）。
		//   Composite は左から順の子（エディタが、子の横の位置で並べ替える）。Branch は [True, False]。
		//   Switch は選び方の出口の数だけ。Root・Decorator は 1 つ
		struct NodeData
		{
			uint32_t id = 0;
			std::string type;
			std::string comment;
			Vector2 position;
			Json params = Json::object();
			std::vector<uint32_t> children;
		};

		// 行動ツリーのアセット（.rdbt。JSON で、エディタのノードの位置も持つ）
		//  ゲームの行動はコードで Registry に登録し、ツリーの形と設定値はこのファイルで決める
		class RD_API TreeAsset
		{
		public:
			static constexpr const char* kExtension = ".rdbt";
			static bool IsTreePath(const std::string& path);
			// Root だけのツリーを作って保存する
			static bool CreateFile(const std::string& path);

			std::vector<NodeData> nodes;
			uint32_t nextId = 1;

			bool Load(const std::string& path);
			bool Save(const std::string& path) const;
			void FromJson(const Json& json);
			Json ToJson() const;

			NodeData* Find(uint32_t id);
			const NodeData* Find(uint32_t id) const;
			const NodeData* GetRoot() const;
			// 子として id を持っているノード（なければ nullptr）。slot にその出口の番号を入れる
			NodeData* FindParent(uint32_t id, int* slot = nullptr);
			NodeData& AddNode(const std::string& type, const Vector2& position);
			void RemoveNode(uint32_t id);
			// 種類に合わせて、出口の数と設定値の欄をそろえる（足りない設定値は既定値で埋める）
			void Normalize(NodeData& node) const;
			// 出口の数（Composite は今つないでいる数。並べ替えのため）
			static int GetFixedSlotCount(const NodeData& node);
			// Composite の子を、子の横の位置の順に並べ替える（変わったら true）
			bool SortChildrenByPosition();
			// 上から下へ木の形に並べ直す（つながっていないノードは右に並べる）
			void AutoLayout(float nodeWidth = 190.0f, float rowHeight = 130.0f);

			// ツリーを作る。outNodes には、アセットのノードの番号 → 作ったノードを入れる（エディタの表示に使う）
			NodePtr Instantiate(Actor& self, std::unordered_map<uint32_t, Node*>* outNodes = nullptr, std::string* outWarnings = nullptr) const;
		};

		// 動いているツリー 1 本（アセットから作り、毎フレーム Tick する）
		//  作ったものは全体の一覧に入り、エディタがアクターから探して動いている様子を描く
		class RD_API TreeInstance
		{
		public:
			TreeInstance();
			~TreeInstance();
			TreeInstance(const TreeInstance&) = delete;
			TreeInstance& operator=(const TreeInstance&) = delete;

			bool Load(const std::string& path, Actor& self);
			void Tick(float dt);
			// アクションの後始末（OnEnd）を呼ばずに捨てる（アクターごと消えるとき）
			void Clear();

			bool IsValid() const { return mRoot != nullptr; }
			const std::string& GetPath() const { return mPath; }
			Actor* GetOwner() const { return mOwner; }
			const Node* FindNode(uint32_t id) const;
			uint64_t GetTickCount() const { return mTicks; }

			static const std::vector<TreeInstance*>& GetAll();

		private:
			std::string mPath;
			Actor* mOwner = nullptr;
			NodePtr mRoot;
			std::unordered_map<uint32_t, Node*> mNodes;
			uint64_t mTicks = 0;
		};
	}
}
