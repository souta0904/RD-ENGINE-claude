#pragma once
#include "Editor/Preview/ModelView.h"

namespace rd
{
	// プレビューパネルのモデル（残りの広さいっぱいに描く。左ドラッグで回す・ホイールで寄る・ダブルクリックで元の向き）
	//  アニメーションがあればクリップを選んで再生できる
	class RD_API ModelPreviewWidget
	{
	public:
		void Draw(const std::string& path);

	private:
		ModelView mView;
		bool mPlaying = true;
	};
}
