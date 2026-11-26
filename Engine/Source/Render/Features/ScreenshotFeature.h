#pragma once
#include "Render/Renderer/RenderFeature.h"
#include "RHI/D3D12Common.h"
#include <string>

namespace rd
{
	class RenderTexture;

	// メインの画面の結果を PNG で保存する
	//  Request で予約すると、次のメインの画面の描画の最後に読み戻し用のバッファへ写し（"Capture" パス）、
	//  GPU が描き終えた後の Finish でファイルに書き出す
	class RD_API ScreenshotFeature final : public RenderFeature
	{
	public:
		void Request(const std::string& path) { mRequestedPath = path; }
		void Finish();

		void CreatePipelines(const RenderShared&) override {}
		void AddPasses(rg::RenderGraph& graph, FrameContext& frame) override;

	private:
		// total は最後の行のパディングを含まないので、読む側（RowPitch * Height）に合わせて確保する
		void CopyToReadback(ID3D12GraphicsCommandList* cmd, RenderTexture& output);

		std::string mRequestedPath;
		std::string mPendingPath;
		ComPtr<ID3D12Resource> mBuffer;
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT mFootprint = {};
	};
}
