#pragma once
#include "Render/Material/Material.h"
#include "RHI/D3D12Common.h"

namespace rd
{
	struct RenderStats;

	// どの描画の機能からも使う土台（ルートシグネチャ 2 つと、画面全体に描く関数）
	//  mesh : メッシュ / 空 / パーティクル / ディファードのライティング / 縁取り
	//         b0 Object / b1 Frame / b2 Material / b3 Lights / t0 Albedo / t1 Normal / t2 Palette（またはパーティクル）/ t3 ShadowMap / t4 Emissive
	//         b4 と t5～t8 は自作のシェーダーの Properties（頂点を動かす関数でも読めるように、どの段からも見える）
	//  simple : 2D / デバッグ線 / ポストエフェクト（32bit 定数と t0・t1）
	class RD_API RenderShared
	{
	public:
		void Create();

		ID3D12RootSignature* GetMeshRS() const { return mMeshRS.Get(); }
		ID3D12RootSignature* GetSimpleRS() const { return mSimpleRS.Get(); }

		// 画面全体を覆う 1 枚の三角形で ps を描く（Post.hlsl のポストエフェクト用のパイプラインを作る）
		ComPtr<ID3D12PipelineState> BuildPostPipeline(const char* ps, DXGI_FORMAT format, BlendMode blend, const wchar_t* name) const;
		// 画面全体を描く（描く先・ビューポート・ルートシグネチャは呼ぶ側が置く）。t1 を省くと t0 と同じものを置く
		void DrawFullscreen(ID3D12GraphicsCommandList* cmd, ID3D12PipelineState* pso, const Vector4& params,
			D3D12_GPU_DESCRIPTOR_HANDLE t0, RenderStats& stats, D3D12_GPU_DESCRIPTOR_HANDLE t1 = {}) const;

	private:
		ComPtr<ID3D12RootSignature> mMeshRS;
		ComPtr<ID3D12RootSignature> mSimpleRS;
	};
}
