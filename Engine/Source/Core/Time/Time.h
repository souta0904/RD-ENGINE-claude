#pragma once
#include <cstdint>

namespace rd
{
	// ゲーム時間（Unity の Time と同じ使い方。どこからでも Time::DeltaTime() で読める）
	//
	//  値を書き換えるのはエンジン（Engine と PlayController）だけで、毎フレームの最初に更新される。
	//  ゲームのコードからは読むだけ（変えられるのは TimeScale と FixedDeltaTime だけ）。
	//
	//  使い分け
	//    DeltaTime          … 動きの計算に使う。pos += speed * Time::DeltaTime() のように掛けると、
	//                         FPS が変わっても 1 秒あたりの移動量が同じになる
	//    UnscaledDeltaTime  … TimeScale の影響を受けない。ポーズ中も動かしたい UI や演出に使う
	//    FixedDeltaTime     … 物理の 1 ステップの時間（既定は 1/60 秒）。物理は毎フレームではなく、この間隔で進める
	//    TimeScale          … 時間の速さ。0 で止まり、0.5 でスロー、2 で倍速（ヒットストップやポーズに使う）
	class RD_API Time
	{
	public:
		// 前のフレームからの秒数（TimeScale を掛けた値）
		static float DeltaTime() { return sDelta; }
		// 前のフレームからの実際の秒数（TimeScale を掛けない）
		static float UnscaledDeltaTime() { return sUnscaledDelta; }
		static float FixedDeltaTime() { return sFixedDelta; }
		static float TimeScale() { return sTimeScale; }
		// プレイを始めてからのゲーム時間の合計（TimeScale の影響を受ける）
		// double にしているのは、float だと何時間も動かしたときに小さな DeltaTime を足しても値が増えなくなるため
		static double TotalTime() { return sTotal; }
		// 起動してからの実際の時間（ポーズしても進む）
		static double RealTime() { return sRealTime; }
		// 起動してから何フレーム目か
		static uint64_t FrameCount() { return sFrameCount; }

		// 負の速さは意味がないので 0 で止める
		static void SetTimeScale(float scale) { sTimeScale = scale < 0.0f ? 0.0f : scale; }
		static void SetFixedDeltaTime(float dt) { sFixedDelta = dt; }

	private:
		// friend にしたクラスだけが private の値を書き換えられる（ゲームのコードから勝手に変えられないように）
		friend class Engine;
		friend class PlayController;
		// static inline … ヘッダーの中で static 変数の実体まで書ける書き方（.cpp に定義を書かなくてよい）
		static inline float sDelta = 0.0f;
		static inline float sUnscaledDelta = 0.0f;
		static inline float sFixedDelta = 1.0f / 60.0f;
		static inline float sTimeScale = 1.0f;
		static inline double sTotal = 0.0;
		static inline double sRealTime = 0.0;
		static inline uint64_t sFrameCount = 0;
	};
}
