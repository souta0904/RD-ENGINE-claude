#pragma once
#include "Editor/Preview/PrefabView.h"

namespace rd
{
	// プレビューパネルのプレハブ（操作はモデルと同じ）
	class RD_API PrefabPreviewWidget
	{
	public:
		void Draw(const std::string& path);
		void Reload() { mView.Reload(); }

	private:
		PrefabView mView;
	};
}
