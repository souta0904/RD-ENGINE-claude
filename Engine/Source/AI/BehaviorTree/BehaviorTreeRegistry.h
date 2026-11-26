#pragma once
#include "AI/BehaviorTree/BehaviorTree.h"
#include "Core/Serialization/Json.h"
#include <functional>
#include <string>
#include <vector>

namespace rd
{
	class Actor;

	namespace bt
	{
		// ノードの設定値の種類（エディタでの入力欄の形が決まる）
		enum class ParamType { Bool, Int, Float, String, Enum, Condition, Selector };

		struct ParamDef
		{
			std::string name;
			ParamType type = ParamType::Float;
			Json defaultValue;
			std::vector<std::string> options; // Enum の選択肢
			std::string tooltip;
		};

		// ノードの形（子のつなぎ方）
		//  Root: 一番上（子 1 つ） / Composite: 子をいくつでも（左から順） / Decorator: 子 1 つ /
		//  Branch: 子 2 つ（True / False） / Switch: 選び方の値ごとに子 1 つ / Leaf: 子なし
		enum class NodeKind { Root, Composite, Decorator, Branch, Switch, Leaf };

		// 葉のノード（アクション）を作る関数。self はツリーを動かしているアクター、params はノードの設定値
		//  作れなければ nullptr（ツリーを作るときに警告を出し、失敗するだけのノードに置き換える）
		using LeafFactory = std::function<NodePtr(Actor& self, const Json& params)>;
		using ConditionFactory = std::function<Condition(Actor& self)>;
		using SelectorFactory = std::function<std::function<int()>(Actor& self)>;

		struct NodeTypeInfo
		{
			std::string name;        // ファイルに書く名前（"Sequence"・"Chase" など）
			std::string category;    // エディタの作成メニューでの分類（"Composite"・"Cat" など）
			NodeKind kind = NodeKind::Leaf;
			std::string description;
			std::vector<ParamDef> params;
			LeafFactory create;      // Leaf のときだけ（組み込みの Composite などはツリーの組み立て側で作る）
			int module = 0;
		};

		// ガードや分岐で使う、名前の付いた条件（例: "IsTargetFound"）
		struct ConditionInfo
		{
			std::string name;
			std::string description;
			ConditionFactory create;
			int module = 0;
		};

		// Switch で使う、名前の付いた選び方（例: "SearchState"）。cases は出口の名前
		struct SelectorInfo
		{
			std::string name;
			std::vector<std::string> cases;
			std::string description;
			SelectorFactory create;
			int module = 0;
		};

		// ノードの種類・条件・選び方の一覧
		//  組み込み（Sequence・Selector・Guard・Wait など）はエンジンが登録する。
		//  ゲームは .cpp の static の初期化などで RegisterAction / RegisterCondition / RegisterSelector を呼ぶ。
		//  ゲームのコードを読み直すときは、ComponentRegistry と同じく、ゲームが登録したものをいったん外す
		class RD_API Registry
		{
		public:
			static void RegisterAction(NodeTypeInfo info);
			static void RegisterCondition(ConditionInfo info);
			static void RegisterSelector(SelectorInfo info);

			static const NodeTypeInfo* FindType(const std::string& name);
			static const ConditionInfo* FindCondition(const std::string& name);
			static const SelectorInfo* FindSelector(const std::string& name);
			static const std::vector<NodeTypeInfo>& GetTypes();
			static const std::vector<ConditionInfo>& GetConditions();
			static const std::vector<SelectorInfo>& GetSelectors();

			struct Saved
			{
				std::vector<NodeTypeInfo> types;
				std::vector<ConditionInfo> conditions;
				std::vector<SelectorInfo> selectors;
			};
			static Saved RemoveGameEntries();
			static void RestoreEntries(Saved saved);
		};

		// 組み込みのノードの名前
		namespace builtin
		{
			inline constexpr const char* kRoot = "Root";
			inline constexpr const char* kSequence = "Sequence";
			inline constexpr const char* kSelector = "Selector";
			inline constexpr const char* kRepeat = "Repeat";
			inline constexpr const char* kInverter = "Inverter";
			inline constexpr const char* kGuard = "Guard";
			inline constexpr const char* kBranch = "Branch";
			inline constexpr const char* kSwitch = "Switch";
			inline constexpr const char* kCheck = "Check";
			inline constexpr const char* kWait = "Wait";
			inline constexpr const char* kLog = "Log";
			inline constexpr const char* kSucceed = "Succeed";
			inline constexpr const char* kFail = "Fail";
		}

		// 設定値を読む（なければ既定値）
		inline float GetFloat(const Json& params, const char* name, float fallback = 0.0f)
		{
			auto it = params.find(name);
			return (it != params.end() && it->is_number()) ? it->get<float>() : fallback;
		}
		inline int GetInt(const Json& params, const char* name, int fallback = 0)
		{
			auto it = params.find(name);
			return (it != params.end() && it->is_number()) ? it->get<int>() : fallback;
		}
		inline bool GetBool(const Json& params, const char* name, bool fallback = false)
		{
			auto it = params.find(name);
			return (it != params.end() && it->is_boolean()) ? it->get<bool>() : fallback;
		}
		inline std::string GetString(const Json& params, const char* name, const std::string& fallback = {})
		{
			auto it = params.find(name);
			return (it != params.end() && it->is_string()) ? it->get<std::string>() : fallback;
		}
	}
}
