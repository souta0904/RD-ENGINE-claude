#pragma once
#include "Render/Camera/RenderCamera.h"
#include "Render/Debug/DebugDraw.h"
#include "Render/GpuData.h"
#include "Render/Graph/RenderGraph.h"
#include "Render/RenderList/RenderList.h"
#include "Render/Settings/Environment.h"
#include "Render/Settings/RenderSettings.h"
#include "Render/Settings/RenderStats.h"
#include "RHI/Texture/DepthTexture.h"
#include "RHI/Texture/RenderTexture.h"
#include <vector>

namespace rd
{
	class RenderShared;

	// 描画の機能どうしでやり取りする、レンダーグラフのテクスチャ（無いものは IsValid() が false）
	struct FrameTextures
	{
		rg::TextureHandle output; // 最後の結果（sRGB の LDR）
		rg::TextureHandle depth;  // 不透明の深度
		rg::TextureHandle hdr;    // 光を当てた色（トーンマップの前）
		rg::TextureHandle shadow; // 影の深度（カスケードを 2x2 に並べたもの）
		rg::TextureHandle bloom;  // ブルームのいちばん大きい段（ブルームを切っていれば無い）
	};

	// 1 回の描画（Renderer::Render）のあいだ、描画の機能に渡す値
	//  Renderer が作って値を入れ、機能はここから読み、テクスチャを足していく。
	//  グラフのパスの中身（実行）も同じ Render の中で呼ばれるので、パスから参照してよい。
	struct FrameContext
	{
		const RenderShared& shared;
		const RenderCamera& camera;
		const Environment& env;
		const RenderSettings& settings;
		RenderList& list;
		DebugDraw& debug;
		RenderStats& stats;
		uint32_t width = 1;
		uint32_t height = 1;
		// メインの画面を描くときだけ true（スクリーンショットはメインの画面だけ撮る）
		bool isMain = true;

		// シェーダーに渡すフレームとライトの値（GPU へ送ったアドレスも）
		GpuFrameData data;
		D3D12_GPU_VIRTUAL_ADDRESS frameCB = 0;
		D3D12_GPU_VIRTUAL_ADDRESS lightCB = 0;
		bool shadowActive = false;

		// カメラに映るメッシュ（不透明は手前から、半透明は奥から）
		std::vector<const MeshDrawItem*> opaque;
		std::vector<const MeshDrawItem*> transparent;

		FrameTextures textures;

		// メッシュ用のルートシグネチャと、フレーム・ライト・影の定数を置く
		void BindMeshPass(ID3D12GraphicsCommandList* cmd, const GpuTexture& shadowMap) const;
	};
}
