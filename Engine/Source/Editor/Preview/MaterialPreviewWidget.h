#pragma once
#include "Editor/Preview/MaterialView.h"

namespace rd
{
	// プレビューパネルのマテリアル（操作はモデルと同じ）
	class RD_API MaterialPreviewWidget
	{
	public:
		void Draw(const std::string& path);

	private:
		MaterialView mView;
	};
}
