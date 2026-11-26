#pragma once
#include "Runtime/FrameOverlay.h"

namespace rd
{
	struct EngineConfig;
	struct WindowDesc;

	// エンジンに外から差し込む層（エディタが実装する。Unreal の UEditorEngine が UEngine に足す部分にあたる）
	//  Engine はこの形しか知らないので、Engine も Runtime もエディタに依存しない。
	//  差し込む層は EngineConfig::createLayer で作る（エディタ入りの構成で、-noeditor でないとき）。
	//  層があるとき、プレイを始めるかどうかは層が決める（ないときはいつもプレイ中）。
	//
	//  呼ばれる順:
	//   OnConfigure（窓を作る前。窓の位置や最初のシーンを変えられる）
	//   → OnInitialize（サブシステムと最初のシーンができた後）
	//   → 毎フレーム OnBeginFrame・OnUpdate（シーンの更新の前）と、FrameOverlay の描画の関数
	//   → OnShutdown（シーンを消す前）→ 層を消す
	//  OnSceneChanged はシーンがフレームの区切りで差し替わったとき（LoadScene など）に呼ばれる。
	class EngineLayer : public FrameOverlay
	{
	public:
		virtual void OnConfigure(EngineConfig& /*config*/, WindowDesc& /*window*/) {}
		virtual void OnInitialize() {}
		virtual void OnShutdown() {}
		// 窓のメッセージを先に受け取る（true を返すとエンジンは処理しない）
		virtual bool HandleMessage(HWND /*hwnd*/, UINT /*msg*/, WPARAM /*wParam*/, LPARAM /*lParam*/) { return false; }
		virtual void OnBeginFrame() {}
		virtual void OnUpdate(float /*dt*/) {}
		virtual void OnSceneChanged() {}
	};
}
