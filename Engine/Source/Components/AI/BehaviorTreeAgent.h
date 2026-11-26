#pragma once
#include "AI/BehaviorTree/BehaviorTreeAsset.h"
#include "Scene/Component/Component.h"
#include <string>

namespace rd
{
	// 行動ツリーのアセット（.rdbt）を読んで、毎フレーム動かす
	//  ツリーの形はエディタの「行動ツリー」パネルで組み、行動はゲームのコードで bt::Registry に登録する
	class RD_API BehaviorTreeAgent final : public Component
	{
		RD_COMPONENT(BehaviorTreeAgent)
	public:
		std::string tree;

		void Reflect(PropertyVisitor& v) override;
		void Start() override;
		void Update(float dt) override;
		void OnDestroy() override;

		const bt::TreeInstance& GetInstance() const { return mInstance; }

	private:
		bt::TreeInstance mInstance;
	};
}
