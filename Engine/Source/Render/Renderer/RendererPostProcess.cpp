#include "RdPch.h"
#include "Render/Renderer/Renderer.h"
#include "Asset/AssetManager.h"
#include "Core/Util/StringUtil.h"
#include "Math/Matrix3.h"
#include "Render/Renderer/RendererCommon.h"
#include "RHI/Device/GpuScope.h"
#include <DirectXTex.h>

namespace rd
{
	using namespace render;

	void Renderer::DrawFullscreen(ID3D12GraphicsCommandList* cmd, ID3D12PipelineState* pso, const Vector4& params,
		D3D12_GPU_DESCRIPTOR_HANDLE t0, D3D12_GPU_DESCRIPTOR_HANDLE t1)
	{
		if (!pso) { return; }
		SimpleConstants c;
		c.params = params;
		cmd->SetPipelineState(pso);
		cmd->SetGraphicsRoot32BitConstants(kSimpleConstants, sizeof(c) / 4, &c, 0);
		cmd->SetGraphicsRootDescriptorTable(kSimpleTex0, t0);
		cmd->SetGraphicsRootDescriptorTable(kSimpleTex1, t1.ptr ? t1 : t0);
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmd->DrawInstanced(3, 1, 0, 0);
		++mStats.drawCalls;
	}

	void Renderer::RenderToBloom(ID3D12GraphicsCommandList* cmd, RenderTexture& target)
	{
		target.Transition(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);
		auto rtv = target.GetRtv();
		cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
		SetViewport(cmd, target.GetWidth(), target.GetHeight());
	}

	// 明るい部分を取り出して半分に縮め、さらに段階的に縮めてから、拡大しながら上の段へ足し戻す
	void Renderer::BloomPass(ID3D12GraphicsCommandList* cmd)
	{
		RD_GPU_SCOPE("Bloom");
		cmd->SetGraphicsRootSignature(mSimpleRS.Get());
		if (!mSettings.bloom) { return; }

		RenderToBloom(cmd, mT->bloom[0]);
		DrawFullscreen(cmd, mBloomPrefilterPSO.Get(), Vector4(mSettings.bloomThreshold, mSettings.bloomKnee, 0, 0), mT->hdr.GetGpuSrv());
		mT->bloom[0].Transition(cmd, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		for (uint32_t i = 1; i < RenderTargets::kBloomLevels; ++i)
		{
			RenderToBloom(cmd, mT->bloom[i]);
			DrawFullscreen(cmd, mBloomDownPSO.Get(), Vector4(), mT->bloom[i - 1].GetGpuSrv());
			mT->bloom[i].Transition(cmd, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}
		for (uint32_t i = RenderTargets::kBloomLevels - 1; i > 0; --i)
		{
			RenderToBloom(cmd, mT->bloom[i - 1]);
			DrawFullscreen(cmd, mBloomUpPSO.Get(), Vector4(1.0f, 0, 0, 0), mT->bloom[i].GetGpuSrv());
			mT->bloom[i - 1].Transition(cmd, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}
	}

	void Renderer::TonemapPass(ID3D12GraphicsCommandList* cmd)
	{
		RD_GPU_SCOPE("Tonemap");
		cmd->SetGraphicsRootSignature(mSimpleRS.Get());
		mT->output.Transition(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);
		auto rtv = mT->output.GetRtv();
		cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
		SetViewport(cmd, mT->output.GetWidth(), mT->output.GetHeight());
		if (!mTonemapPSO) { return; }
		// トーンマップの方式は 0 = ACES, 1 = Neutral, 2 = なし（Render Settings で切ってあればなし）
		float tonemapper = mSettings.tonemap ? float(int(mEnv.tonemapper)) : 2.0f;
		SimpleConstants c;
		c.params = Vector4(mSettings.exposure, mSettings.bloom ? mSettings.bloomIntensity : 0.0f, mSettings.vignette, tonemapper);
		// 行列の場所に色の調整とレトロ表示の値を入れる（シェーダーでは gExtra[0], gExtra[1]）
		float* extra = c.matrix.Data();
		extra[0] = std::exp2(mEnv.postExposure);
		extra[1] = 1.0f + mEnv.contrast / 100.0f;
		extra[2] = 1.0f + mEnv.saturation / 100.0f;
		extra[3] = 0.0f;
		extra[4] = float(mEnv.retroHeight);
		extra[5] = float(std::max(2u, mEnv.retroColorLevels));
		extra[6] = 0.0f;
		extra[7] = 0.0f;
		// Unity の Vignette（gExtra[2] = 色と縦横比、gExtra[3] = 中心・強さ×3・なめらかさ×5。URP と同じ値の渡し方）
		float aspect = float(mT->output.GetWidth()) / float(std::max(mT->output.GetHeight(), 1u));
		extra[8] = mEnv.vignetteColor.r;
		extra[9] = mEnv.vignetteColor.g;
		extra[10] = mEnv.vignetteColor.b;
		extra[11] = mEnv.vignetteRounded ? aspect : 1.0f;
		extra[12] = mEnv.vignetteCenter.x;
		extra[13] = mEnv.vignetteCenter.y;
		extra[14] = mEnv.vignetteIntensity * 3.0f;
		extra[15] = mEnv.vignetteSmoothness * 5.0f;
		cmd->SetPipelineState(mTonemapPSO.Get());
		cmd->SetGraphicsRoot32BitConstants(kSimpleConstants, sizeof(c) / 4, &c, 0);
		cmd->SetGraphicsRootDescriptorTable(kSimpleTex0, mT->hdr.GetGpuSrv());
		// ブルームがないときはブルームの代わりに HDR を渡し、強さ 0 で使わない
		cmd->SetGraphicsRootDescriptorTable(kSimpleTex1, mSettings.bloom ? mT->bloom[0].GetGpuSrv() : mT->hdr.GetGpuSrv());
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmd->DrawInstanced(3, 1, 0, 0);
		++mStats.drawCalls;
	}

	void Renderer::OverlayPass(ID3D12GraphicsCommandList* cmd, const Matrix4& viewProj)
	{
		RD_GPU_SCOPE("Overlay");
		auto rtv = mT->output.GetRtv();
		auto dsv = mT->depth.GetDsv();
		cmd->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
		SetViewport(cmd, mT->output.GetWidth(), mT->output.GetHeight());
		cmd->SetGraphicsRootSignature(mSimpleRS.Get());
		// 面を先に描き、その上に線を重ねる（面の縁取りの線が面に埋もれないように）
		constexpr auto kTris = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		constexpr auto kLines = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
		DrawDebugVertices(cmd, mDebug.GetDepthTestedTriangles(), mDebugTriDepthPSO.Get(), viewProj, kTris);
		DrawDebugVertices(cmd, mDebug.GetDepthTested(), mLineDepthPSO.Get(), viewProj, kLines);
		DrawDebugVertices(cmd, mDebug.GetOverlayTriangles(), mDebugTriOverlayPSO.Get(), viewProj, kTris);
		DrawDebugVertices(cmd, mDebug.GetOverlay(), mLineOverlayPSO.Get(), viewProj, kLines);
		DrawSprites(cmd);
	}

	void Renderer::DrawDebugVertices(ID3D12GraphicsCommandList* cmd, const std::vector<DebugDraw::Vertex>& vertices, ID3D12PipelineState* pso,
		const Matrix4& viewProj, D3D12_PRIMITIVE_TOPOLOGY topology)
	{
		if (vertices.empty() || !pso) { return; }
		auto& device = GraphicsDevice::Get();
		uint64_t size = sizeof(DebugDraw::Vertex) * vertices.size();
		D3D12_VERTEX_BUFFER_VIEW vbv = { device.UploadData(vertices.data(), size), UINT(size), sizeof(DebugDraw::Vertex) };
		SimpleConstants c;
		c.matrix = viewProj;
		cmd->SetPipelineState(pso);
		cmd->SetGraphicsRoot32BitConstants(kSimpleConstants, sizeof(c) / 4, &c, 0);
		cmd->IASetPrimitiveTopology(topology);
		cmd->IASetVertexBuffers(0, 1, &vbv);
		cmd->DrawInstanced(UINT(vertices.size()), 1, 0, 0);
		++mStats.drawCalls;
	}

	// レイヤーの順に並べて頂点を作り、同じテクスチャが続く範囲を 1 回の描画にまとめる
	void Renderer::DrawSprites(ID3D12GraphicsCommandList* cmd)
	{
		auto& sprites = mList.sprites;
		if (sprites.empty() || !mSpritePSO) { return; }
		auto& device = GraphicsDevice::Get();
		auto& assets = AssetManager::Get();
		std::stable_sort(sprites.begin(), sprites.end(), [](auto& a, auto& b) { return a.layer < b.layer; });

		std::vector<SpriteVertex> vertices;
		vertices.reserve(sprites.size() * 6);
		for (const auto& s : sprites)
		{
			Matrix3 m = Matrix3::CreateAffine(s.size, s.rotation, s.position);
			Vector2 corners[4] = { { -s.pivot.x, -s.pivot.y }, { 1.0f - s.pivot.x, -s.pivot.y }, { -s.pivot.x, 1.0f - s.pivot.y }, { 1.0f - s.pivot.x, 1.0f - s.pivot.y } };
			Vector2 uvs[4] = { { s.uvRect.x, s.uvRect.y }, { s.uvRect.z, s.uvRect.y }, { s.uvRect.x, s.uvRect.w }, { s.uvRect.z, s.uvRect.w } };
			SpriteVertex v[4];
			for (int i = 0; i < 4; ++i) { v[i] = { corners[i] * m, uvs[i], s.color }; }
			vertices.insert(vertices.end(), { v[0], v[1], v[2], v[2], v[1], v[3] });
		}
		uint64_t size = sizeof(SpriteVertex) * vertices.size();
		D3D12_VERTEX_BUFFER_VIEW vbv = { device.UploadData(vertices.data(), size), UINT(size), sizeof(SpriteVertex) };

		SimpleConstants c;
		c.matrix = Matrix4::CreateOrthographic(0.0f, 0.0f, float(mT->output.GetWidth()), float(mT->output.GetHeight()), 0.0f, 1.0f);
		cmd->SetPipelineState(mSpritePSO.Get());
		cmd->SetGraphicsRoot32BitConstants(kSimpleConstants, sizeof(c) / 4, &c, 0);
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmd->IASetVertexBuffers(0, 1, &vbv);

		size_t start = 0;
		while (start < sprites.size())
		{
			const Texture* tex = sprites[start].texture;
			size_t end = start + 1;
			while (end < sprites.size() && sprites[end].texture == tex) { ++end; }
			cmd->SetGraphicsRootDescriptorTable(kSimpleTex0, TextureSrv(tex, assets.GetWhiteTexture()));
			cmd->DrawInstanced(UINT((end - start) * 6), 1, UINT(start * 6), 0);
			++mStats.drawCalls;
			start = end;
		}
		mStats.sprites = uint32_t(sprites.size());
	}

	void Renderer::BlitToBackBuffer()
	{
		auto& device = GraphicsDevice::Get();
		auto cmd = device.GetCommandList();
		auto rtv = device.GetBackBufferRtv();
		cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
		float black[4] = { 0, 0, 0, 1 };
		cmd->ClearRenderTargetView(rtv, black, 0, nullptr);
		SetViewport(cmd, device.GetWidth(), device.GetHeight());
		cmd->SetGraphicsRootSignature(mSimpleRS.Get());
		if (mT->output.GetResource()) { DrawFullscreen(cmd, mBlitPSO.Get(), Vector4(), mT->output.GetGpuSrv()); }
	}

	// スクリーンショット用に読み戻しのバッファへ写す
	//  total は最後の行のパディングを含まないので、読む側（RowPitch * Height）に合わせて確保する
	void Renderer::CopyOutputForCapture(ID3D12GraphicsCommandList* cmd)
	{
		auto& device = GraphicsDevice::Get();
		auto desc = mT->output.GetResource()->GetDesc();
		UINT64 total = 0;
		device.GetDevice()->GetCopyableFootprints(&desc, 0, 1, 0, &mCaptureFootprint, nullptr, nullptr, &total);
		CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_READBACK);
		UINT64 size = std::max<UINT64>(total, UINT64(mCaptureFootprint.Footprint.RowPitch) * mCaptureFootprint.Footprint.Height);
		auto bufDesc = CD3DX12_RESOURCE_DESC::Buffer(size);
		mCaptureBuffer.Reset();
		RD_CHECK_HR(device.GetDevice()->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &bufDesc,
			D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&mCaptureBuffer)));
		mT->output.Transition(cmd, D3D12_RESOURCE_STATE_COPY_SOURCE);
		CD3DX12_TEXTURE_COPY_LOCATION dst(mCaptureBuffer.Get(), mCaptureFootprint);
		CD3DX12_TEXTURE_COPY_LOCATION src(mT->output.GetResource(), 0);
		cmd->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
		mPendingCapturePath = std::move(mCapturePath);
		mCapturePath.clear();
	}

	void Renderer::FinishCapture()
	{
		if (mPendingCapturePath.empty() || !mCaptureBuffer) { return; }
		GraphicsDevice::Get().WaitIdle();
		uint8_t* data = nullptr;
		D3D12_RANGE range = { 0, SIZE_T(mCaptureFootprint.Footprint.RowPitch) * mCaptureFootprint.Footprint.Height };
		if (SUCCEEDED(mCaptureBuffer->Map(0, &range, reinterpret_cast<void**>(&data))))
		{
			DirectX::Image image = {};
			image.width = mCaptureFootprint.Footprint.Width;
			image.height = mCaptureFootprint.Footprint.Height;
			image.format = DXGI_FORMAT_R8G8B8A8_UNORM;
			image.rowPitch = mCaptureFootprint.Footprint.RowPitch;
			image.slicePitch = image.rowPitch * image.height;
			image.pixels = data;
			std::filesystem::path path(ToWide(mPendingCapturePath));
			if (path.has_parent_path()) { std::filesystem::create_directories(path.parent_path()); }
			HRESULT hr = DirectX::SaveToWICFile(image, DirectX::WIC_FLAGS_NONE,
				DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG), path.c_str());
			mCaptureBuffer->Unmap(0, nullptr);
			if (SUCCEEDED(hr)) { Logger::Info(LogCat::Render, "Saved screenshot: {}", mPendingCapturePath); }
			else { Logger::Error(LogCat::Render, "Failed to save screenshot: {}", mPendingCapturePath); }
		}
		mPendingCapturePath.clear();
		mCaptureBuffer.Reset();
	}
}
