#pragma once

namespace rd
{
	class PreviewView;

	// プレビューを残りの広さいっぱいに描き、マウスで見る向きを変えられるようにする（プレビューパネルの 3 つの部品で共通）
	//  左ドラッグで回す・ホイールで寄る・ダブルクリックで元の向き
	void DrawOrbitView(PreviewView& view);
}
