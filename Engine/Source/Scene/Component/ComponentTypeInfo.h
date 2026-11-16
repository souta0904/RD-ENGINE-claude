#pragma once
#include <cstdint>
#include <typeindex>

namespace rd
{
	// コンポーネントが上書きしているコールバック
	//  シーンは上書きしているものだけを型ごとのリストに入れて呼ぶので、
	//  Update を持たないコンポーネント（ライトなど）を毎フレーム呼ぶことはない
	enum ComponentCallback : uint32_t
	{
		kCallbackUpdate = 1 << 0,
		kCallbackFixedUpdate = 1 << 1,
		kCallbackLateUpdate = 1 << 2,
		kCallbackRender = 1 << 3,
		kCallbackEditorUpdate = 1 << 4,
		kCallbackGizmos = 1 << 5,
	};

	// コンポーネントの型ごとの情報（RD_COMPONENT が用意する。callbacks は ComponentCallback の組み合わせ）
	struct ComponentTypeInfo
	{
		const char* name;
		std::type_index type;
		uint32_t callbacks;
	};
}
