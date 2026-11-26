#pragma once
#include "Scene/Component/Component.h"

namespace rd
{
	// 子の UI をまとめて透明にする（Unity の CanvasGroup の Alpha）
	//  Canvas が描くときに、このコンポーネントを持つアクターから下の UI に alpha を掛ける
	class RD_API CanvasGroup final : public Component
	{
		RD_COMPONENT(CanvasGroup)
	public:
		float alpha = 1.0f;
		void Reflect(PropertyVisitor& v) override;
	};
}
