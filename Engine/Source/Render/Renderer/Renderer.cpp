#include "RdPch.h"
#include "Render/Renderer/Renderer.h"
#include "Asset/AssetManager.h"
#include "Asset/Model/Model.h"
#include "Render/Material/ShaderAsset.h"
#include "Render/Renderer/RendererCommon.h"
#include "RHI/Device/GpuScope.h"

namespace rd
{
	using namespace render;

	namespace
	{
		Vector3 ToVector3(const Color& c) { return { c.r, c.g, c.b }; }

		// 不透明はパイプライン（スキンの有無）ごとにまとめてから手前から、半透明は奥から描く
		bool OpaqueOrder(const MeshDrawItem* a, const MeshDrawItem* b)
		{
			bool sa = a->palette != 0, sb = b->palette != 0;
			return sa != sb ? sa < sb : a->sortKey < b->sortKey;
		}
		bool TransparentOrder(const MeshDrawItem* a, const MeshDrawItem* b) { return a->sortKey > b->sortKey; }
		bool ShadowCasterFirst(const GpuDirLight& a, const GpuDirLight& b) { return a.castShadow > b.castShadow; }
	}

	void Renderer::Initialize()
	{
		RD_ASSERT(!sInstance);
		sInstance = this;
		CreateRootSignatures();
		CreatePipelines();
		// カスケードを 2x2 のタイルに並べる
		mShadowMap.Create(mSettings.shadowMapSize * 2, mSettings.shadowMapSize * 2, L"ShadowMap");
	}

	void Renderer::Shutdown()
	{
		mMeshPipelines.clear();
		sInstance = nullptr;
	}

	// 自作のシェーダーは Properties も読み直す（足した値がマテリアルの画面に出るように）
	void Renderer::ReloadShaders()
	{
		GraphicsDevice::Get().WaitIdle();
		GraphicsDevice::Get().GetShaderCompiler().ClearCache();
		mMeshPipelines.clear();
		for (const auto& asset : AssetManager::Get().GetAllAssets())
		{
			if (auto shader = std::dynamic_pointer_cast<ShaderAsset>(asset)) { shader->Reload(); }
		}
		CreatePipelines();
		Logger::Info(LogCat::Render, "Shaders reloaded");
	}

	void Renderer::EnsureTargets(uint32_t width, uint32_t height)
	{
		if (mT->output.GetResource() && mT->output.GetWidth() == width && mT->output.GetHeight() == height) { return; }
		mT->output.Create({ width, height, DXGI_FORMAT_R8G8B8A8_TYPELESS, kOutputRtvFormat, DXGI_FORMAT_R8G8B8A8_UNORM, Color(0, 0, 0, 1), L"Output" });
	}

	void Renderer::Render(const RenderCamera& camera, const Environment& env, uint32_t width, uint32_t height, float time)
	{
		RD_GPU_SCOPE("Render Scene");
		auto& device = GraphicsDevice::Get();
		auto cmd = device.GetCommandList();
		mStats = {};
		width = std::max(1u, width);
		height = std::max(1u, height);
		EnsureTargets(width, height);
		mSettings.shadowMapSize = std::clamp(mSettings.shadowMapSize, 256u, 4096u);
		mSettings.shadowCascades = std::clamp(mSettings.shadowCascades, 1u, kMaxShadowCascades);
		if (mShadowMap.GetWidth() != mSettings.shadowMapSize * 2)
		{
			mShadowMap.Resize(mSettings.shadowMapSize * 2, mSettings.shadowMapSize * 2);
		}

		mEnv = env;
		SetupFrameData(camera, env, width, height, time);
		GpuLightData lights = SetupLights();
		mShadowActive = mFrameData.dirLightCount > 0 && lights.dirLights[0].castShadow;
		if (mShadowActive) { SetupShadowCascades(camera, Normalize(lights.dirLights[0].direction), mFrameData); }
		mFrameCB = device.UploadConstants(mFrameData);
		mLightCB = device.UploadConstants(lights);

		DrawList opaque, transparent;
		CasterLists casters;
		CullAndSort(camera, opaque, transparent, casters);

		// パスを登録して、飛ばすものと使い回すものを決めてから実行する（パスの中身は Execute の間に呼ばれる）
		mGraph.Reset();
		BuildGraph(env, width, height, opaque, transparent, casters);
		mGraph.Compile();
		if (mT == &mMain) { mGraphInfo = mGraph.GetInfo(); }
		mGraph.Execute(cmd);

		mList.Clear();
		mDebug.Clear();
	}

	void Renderer::BuildGraph(const Environment& env, uint32_t width, uint32_t height, const DrawList& opaque, const DrawList& transparent,
		const CasterLists& casters)
	{
		using namespace rg;
		RenderGraph& g = mGraph;
		constexpr auto kShaderResource = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		const bool deferred = mSettings.path == RenderPath::Deferred && mDeferredLightingPSO;

		TextureHandle output = g.Import("Output", &mT->output, kShaderResource, true);
		TextureHandle shadow = g.Import("Shadow Map", &mShadowMap, kShaderResource, false, true);
		TextureDesc depthDesc;
		depthDesc.width = width;
		depthDesc.height = height;
		depthDesc.depth = true;
		TextureHandle depth = g.Create("Depth", depthDesc);
		TextureHandle hdr = g.Create("HDR", { width, height, kHdrFormat, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, false, Color(0, 0, 0, 1) });

		if (mShadowActive)
		{
			g.AddPass("Shadow", [&](PassBuilder& b) { b.WriteDepth(shadow, true); },
				[this, shadow, &casters](PassContext& c) { ShadowPass(c.cmd, c.Depth(shadow), casters); });
		}

		if (!deferred)
		{
			// フォワード: 不透明 → 空 → 半透明 / パーティクルを 1 つのパスで描く
			g.AddPass("Forward", [&](PassBuilder& b) { b.WriteNew(hdr); b.WriteDepth(depth, true); b.Read(shadow); },
				[this, hdr, depth, shadow, width, height, &env, &opaque, &transparent](PassContext& c)
				{
					RenderTexture& target = c.Color(hdr);
					DepthTexture& depthTex = c.Depth(depth);
					auto rtv = target.GetRtv();
					auto dsv = depthTex.GetDsv();
					c.cmd->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
					Color clear(mFrameData.skyHorizon.x, mFrameData.skyHorizon.y, mFrameData.skyHorizon.z, 1.0f);
					c.cmd->ClearRenderTargetView(rtv, clear.Data(), 0, nullptr);
					depthTex.Clear(c.cmd);
					BindMeshPass(c.cmd, c.Texture(shadow), width, height);
					DrawMeshes(c.cmd, opaque, MeshPass::Forward);
					DrawSkyAndTransparent(c.cmd, env, transparent);
				});
		}
		else
		{
			// ディファード: 不透明を G バッファーへ書く → 画面全体で光を当てる → 空と半透明を重ねる
			const char* names[kGBufferCount] = { "GBuffer Albedo", "GBuffer Normal", "GBuffer Emission" };
			std::array<TextureHandle, kGBufferCount> gbuffer;
			for (uint32_t i = 0; i < kGBufferCount; ++i)
			{
				gbuffer[i] = g.Create(names[i], { width, height, kGBufferFormat, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, false, Color(0, 0, 0, 0) });
			}
			g.AddPass("GBuffer", [&](PassBuilder& b) { for (auto h : gbuffer) { b.WriteNew(h); } b.WriteDepth(depth, true); b.Read(shadow); },
				[this, gbuffer, depth, shadow, width, height, &opaque](PassContext& c)
				{
					D3D12_CPU_DESCRIPTOR_HANDLE rtvs[kGBufferCount];
					for (uint32_t i = 0; i < kGBufferCount; ++i)
					{
						RenderTexture& t = c.Color(gbuffer[i]);
						rtvs[i] = t.GetRtv();
						t.Clear(c.cmd);
					}
					DepthTexture& depthTex = c.Depth(depth);
					auto dsv = depthTex.GetDsv();
					c.cmd->OMSetRenderTargets(kGBufferCount, rtvs, FALSE, &dsv);
					depthTex.Clear(c.cmd);
					BindMeshPass(c.cmd, c.Texture(shadow), width, height);
					DrawMeshes(c.cmd, opaque, MeshPass::GBuffer);
				});
			g.AddPass("Deferred Lighting", [&](PassBuilder& b) { for (auto h : gbuffer) { b.Read(h); } b.Read(depth); b.Read(shadow); b.WriteNew(hdr); },
				[this, gbuffer, depth, shadow, hdr, width, height](PassContext& c)
				{
					const GpuTexture* gb[kGBufferCount];
					for (uint32_t i = 0; i < kGBufferCount; ++i) { gb[i] = &c.Texture(gbuffer[i]); }
					DeferredLightingPass(c.cmd, c.Color(hdr), gb, c.Texture(depth), c.Texture(shadow), width, height);
				});
			g.AddPass("Sky & Transparent", [&](PassBuilder& b) { b.Write(hdr); b.ReadDepth(depth); b.Read(shadow); },
				[this, hdr, depth, shadow, width, height, &env, &transparent](PassContext& c)
				{
					auto rtv = c.Color(hdr).GetRtv();
					auto dsv = c.Depth(depth).GetDsv();
					c.cmd->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
					BindMeshPass(c.cmd, c.Texture(shadow), width, height);
					DrawSkyAndTransparent(c.cmd, env, transparent);
				});
		}

		// 縁取り: 形を印に塗ってから、広げた形を印のない所だけ HDR に描く（Unity のステンシルの代わり）
		if (!mList.outlines.empty() && mOutlineMaskPSO[0] && mOutlinePSO[0])
		{
			TextureHandle mask = g.Create("Outline Mask", { width, height, kOutlineMaskFormat, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, false, Color(0, 0, 0, 0) });
			g.AddPass("Outline Mask", [&](PassBuilder& b) { b.WriteNew(mask); b.Read(shadow); },
				[this, mask, shadow, width, height](PassContext& c)
				{
					RenderTexture& m = c.Color(mask);
					auto rtv = m.GetRtv();
					c.cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
					m.Clear(c.cmd);
					DrawOutlines(c.cmd, true, nullptr, c.Texture(shadow), width, height);
				});
			g.AddPass("Outline", [&](PassBuilder& b) { b.Read(mask); b.Write(hdr); b.Read(shadow); },
				[this, mask, hdr, shadow, width, height](PassContext& c)
				{
					auto rtv = c.Color(hdr).GetRtv();
					c.cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
					DrawOutlines(c.cmd, false, &c.Texture(mask), c.Texture(shadow), width, height);
				});
		}

		// ブルーム: 明るい部分を取り出して半分に縮め、さらに段階的に縮めてから、拡大しながら上の段へ足し戻す
		TextureHandle bloomTop;
		if (mSettings.bloom)
		{
			static const char* kDown[kBloomLevels] = { "Bloom Prefilter", "Bloom Down 1", "Bloom Down 2", "Bloom Down 3", "Bloom Down 4" };
			static const char* kUp[kBloomLevels] = { "", "Bloom Up 0", "Bloom Up 1", "Bloom Up 2", "Bloom Up 3" };
			static const char* kTextures[kBloomLevels] = { "Bloom 0", "Bloom 1", "Bloom 2", "Bloom 3", "Bloom 4" };
			std::array<TextureHandle, kBloomLevels> bloom;
			uint32_t w = width, h = height;
			for (uint32_t i = 0; i < kBloomLevels; ++i)
			{
				w = std::max(1u, w / 2);
				h = std::max(1u, h / 2);
				bloom[i] = g.Create(kTextures[i], { w, h, kBloomFormat, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, false, Color(0, 0, 0, 1) });
			}
			g.AddPass(kDown[0], [&](PassBuilder& b) { b.Read(hdr); b.WriteNew(bloom[0]); },
				[this, hdr, dst = bloom[0]](PassContext& c)
				{
					c.cmd->SetGraphicsRootSignature(mSimpleRS.Get());
					RenderToBloom(c.cmd, c.Color(dst));
					DrawFullscreen(c.cmd, mBloomPrefilterPSO.Get(), Vector4(mSettings.bloomThreshold, mSettings.bloomKnee, 0, 0), c.Texture(hdr).GetGpuSrv());
				});
			for (uint32_t i = 1; i < kBloomLevels; ++i)
			{
				g.AddPass(kDown[i], [&](PassBuilder& b) { b.Read(bloom[i - 1]); b.WriteNew(bloom[i]); },
					[this, src = bloom[i - 1], dst = bloom[i]](PassContext& c)
					{
						c.cmd->SetGraphicsRootSignature(mSimpleRS.Get());
						RenderToBloom(c.cmd, c.Color(dst));
						DrawFullscreen(c.cmd, mBloomDownPSO.Get(), Vector4(), c.Texture(src).GetGpuSrv());
					});
			}
			for (uint32_t i = kBloomLevels - 1; i > 0; --i)
			{
				g.AddPass(kUp[i], [&](PassBuilder& b) { b.Read(bloom[i]); b.Write(bloom[i - 1]); },
					[this, src = bloom[i], dst = bloom[i - 1]](PassContext& c)
					{
						c.cmd->SetGraphicsRootSignature(mSimpleRS.Get());
						RenderToBloom(c.cmd, c.Color(dst));
						DrawFullscreen(c.cmd, mBloomUpPSO.Get(), Vector4(1.0f, 0, 0, 0), c.Texture(src).GetGpuSrv());
					});
			}
			bloomTop = bloom[0];
		}

		g.AddPass("Tonemap", [&](PassBuilder& b) { b.Read(hdr); if (bloomTop.IsValid()) { b.Read(bloomTop); } b.WriteNew(output); },
			[this, hdr, bloomTop, output](PassContext& c)
			{
				TonemapPass(c.cmd, c.Color(output), c.Texture(hdr), bloomTop.IsValid() ? &c.Texture(bloomTop) : nullptr);
			});
		g.AddPass("Overlay", [&](PassBuilder& b) { b.Write(output); b.ReadDepth(depth); },
			[this, output, depth](PassContext& c) { OverlayPass(c.cmd, c.Color(output), c.Depth(depth), mFrameData.viewProj); });

		if (!mCapturePath.empty())
		{
			g.AddPass("Capture", [&](PassBuilder& b) { b.CopyFrom(output); b.SideEffect(); },
				[this, output](PassContext& c) { CopyOutputForCapture(c.cmd, c.Color(output)); });
		}
	}

	void Renderer::BindMeshPass(ID3D12GraphicsCommandList* cmd, const GpuTexture& shadowMap, uint32_t width, uint32_t height)
	{
		SetViewport(cmd, width, height);
		cmd->SetGraphicsRootSignature(mMeshRS.Get());
		cmd->SetGraphicsRootConstantBufferView(kRootFrame, mFrameCB);
		cmd->SetGraphicsRootConstantBufferView(kRootLights, mLightCB);
		cmd->SetGraphicsRootDescriptorTable(kRootShadow, shadowMap.GetGpuSrv());
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	// G バッファーは t0 / t1 / t4、深度は t5 に置く（メッシュ用のルートシグネチャをそのまま使う）
	void Renderer::DeferredLightingPass(ID3D12GraphicsCommandList* cmd, RenderTexture& hdr, const GpuTexture* gbuffer[3],
		const GpuTexture& depth, const GpuTexture& shadowMap, uint32_t width, uint32_t height)
	{
		auto rtv = hdr.GetRtv();
		cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
		BindMeshPass(cmd, shadowMap, width, height);
		cmd->SetGraphicsRootConstantBufferView(kRootObject, GraphicsDevice::Get().UploadConstants(GpuObjectData{}));
		cmd->SetGraphicsRootDescriptorTable(kRootAlbedo, gbuffer[0]->GetGpuSrv());
		cmd->SetGraphicsRootDescriptorTable(kRootNormal, gbuffer[1]->GetGpuSrv());
		cmd->SetGraphicsRootDescriptorTable(kRootEmissive, gbuffer[2]->GetGpuSrv());
		cmd->SetGraphicsRootDescriptorTable(kRootShaderTex0, depth.GetGpuSrv());
		cmd->SetPipelineState(mDeferredLightingPSO.Get());
		cmd->DrawInstanced(3, 1, 0, 0);
		++mStats.drawCalls;
	}

	void Renderer::DrawSkyAndTransparent(ID3D12GraphicsCommandList* cmd, const Environment& env, const DrawList& transparent)
	{
		if (env.drawSky && mSkyPSO)
		{
			cmd->SetPipelineState(mSkyPSO.Get());
			cmd->DrawInstanced(3, 1, 0, 0);
			++mStats.drawCalls;
		}
		DrawMeshes(cmd, transparent, MeshPass::Forward);
		DrawParticles(cmd);
		DrawTranslucentTriangles(cmd);
	}

	// 描く先・描画リスト・デバッグ線・統計・スクリーンショットの予約をメインの分と入れ替えて描き、元に戻す
	void Renderer::RenderOffscreen(RenderTargets& targets, RenderList& list, const RenderCamera& camera, const Environment& env,
		uint32_t width, uint32_t height, float time, DebugDraw* debug)
	{
		RD_GPU_SCOPE("Render Offscreen");
		RenderStats stats = mStats;
		std::string capture = std::move(mCapturePath);
		mCapturePath.clear();
		DebugDraw empty;
		DebugDraw& lines = debug ? *debug : empty;
		std::swap(mDebug, lines);
		std::swap(mList, list);
		mT = &targets;

		Render(camera, env, width, height, time);

		mT = &mMain;
		std::swap(mList, list);
		std::swap(mDebug, lines);
		mCapturePath = std::move(capture);
		mStats = stats;
	}

	void Renderer::SetupFrameData(const RenderCamera& camera, const Environment& env, uint32_t width, uint32_t height, float time)
	{
		GpuFrameData& f = mFrameData;
		f = {};
		f.view = camera.view;
		f.proj = camera.proj;
		f.viewProj = camera.view * camera.proj;
		f.invViewProj = Inverse(f.viewProj);
		f.cameraPos = camera.position;
		f.cameraRight = camera.GetRight();
		f.cameraUp = camera.GetUp();
		f.time = time;
		f.ambientSky = ToVector3(env.ambientSky) * env.ambientIntensity;
		f.ambientGround = ToVector3(env.ambientGround) * env.ambientIntensity;
		Vector3 equator = ToVector3(env.ambientEquator) * env.ambientIntensity;
		f.ambientEquator = Vector4(equator.x, equator.y, equator.z, env.ambientTrilight ? 1.0f : 0.0f);
		f.additionalLightsPerVertex = env.additionalLightsPerVertex ? 1u : 0u;
		f.lightsPerObject = std::min(env.lightsPerObject, kMaxObjectLights);
		f.skyZenith = ToVector3(env.skyZenith);
		f.skyHorizon = ToVector3(env.skyHorizon);
		f.fogColor = ToVector3(env.fogColor);
		f.fogDensity = env.fogDensity;
		f.fogStart = env.fogStart;
		f.screenSize = Vector2(float(width), float(height));
		f.shadowBias = mSettings.shadowBias;
		f.shadowStrength = mSettings.shadowStrength;
	}

	// 影を落とせるのは先頭の平行光源 1 つだけなので、影を落とすものを先頭へ並べ替える
	GpuLightData Renderer::SetupLights()
	{
		GpuFrameData& f = mFrameData;
		GpuLightData lights = {};
		f.dirLightCount = std::min<uint32_t>(uint32_t(mList.dirLights.size()), kMaxDirLights);
		f.pointLightCount = std::min<uint32_t>(uint32_t(mList.pointLights.size()), kMaxPointLights);
		f.spotLightCount = std::min<uint32_t>(uint32_t(mList.spotLights.size()), kMaxSpotLights);
		std::stable_sort(mList.dirLights.begin(), mList.dirLights.end(), ShadowCasterFirst);
		for (uint32_t i = 0; i < f.dirLightCount; ++i)
		{
			lights.dirLights[i] = mList.dirLights[i];
			lights.dirLights[i].castShadow = (i == 0 && mList.dirLights[i].castShadow && mSettings.shadows) ? 1u : 0u;
		}
		for (uint32_t i = 0; i < f.pointLightCount; ++i) { lights.pointLights[i] = mList.pointLights[i]; }
		for (uint32_t i = 0; i < f.spotLightCount; ++i) { lights.spotLights[i] = mList.spotLights[i]; }
		mStats.lights = f.dirLightCount + f.pointLightCount + f.spotLightCount;
		return lights;
	}

	// 影を落とす物はカスケードごとに、その範囲に入るものだけを集める（カメラに映らない物も含む）
	void Renderer::CullAndSort(const RenderCamera& camera, DrawList& opaque, DrawList& transparent, CasterLists& casters)
	{
		RD_PROFILE_SCOPE("Cull & Sort");
		Frustum frustum = Frustum::FromViewProj(mFrameData.viewProj);
		uint32_t casterCount = 0;
		for (auto& item : mList.meshes)
		{
			if (!item.model || !item.model->IsReady() || !item.material) { continue; }
			if (mShadowActive && item.material->castShadows && item.material->blend == BlendMode::Opaque)
			{
				bool any = false;
				for (uint32_t c = 0; c < mFrameData.cascadeCount; ++c)
				{
					if (mCascadeFrustums[c].Intersects(item.worldBounds)) { casters[c].push_back(&item); any = true; }
				}
				if (any) { ++casterCount; }
			}
			if (mSettings.frustumCulling && !frustum.Intersects(item.worldBounds))
			{
				++mStats.meshesCulled;
				continue;
			}
			item.sortKey = LengthSq(item.worldBounds.GetCenter() - camera.position);
			(item.material->blend == BlendMode::Opaque ? opaque : transparent).push_back(&item);
		}
		std::sort(opaque.begin(), opaque.end(), OpaqueOrder);
		std::sort(transparent.begin(), transparent.end(), TransparentOrder);
		mStats.meshesVisible = uint32_t(opaque.size() + transparent.size());
		mStats.shadowCasters = casterCount;
	}

	// ルートシグネチャを Simple に切り替えるので、メインパスの最後に描く
	void Renderer::DrawTranslucentTriangles(ID3D12GraphicsCommandList* cmd)
	{
		auto& tris = mList.translucentTriangles;
		if (tris.size() < 3 || !mTranslucentTrianglePSO) { return; }
		auto& device = GraphicsDevice::Get();
		UINT count = UINT(tris.size() / 3 * 3);
		uint64_t size = sizeof(ColorVertex) * count;
		D3D12_VERTEX_BUFFER_VIEW vbv = { device.UploadData(tris.data(), size), UINT(size), sizeof(ColorVertex) };
		SimpleConstants c;
		c.matrix = mFrameData.viewProj;
		cmd->SetGraphicsRootSignature(mSimpleRS.Get());
		cmd->SetPipelineState(mTranslucentTrianglePSO.Get());
		cmd->SetGraphicsRoot32BitConstants(kSimpleConstants, sizeof(c) / 4, &c, 0);
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmd->IASetVertexBuffers(0, 1, &vbv);
		cmd->DrawInstanced(count, 1, 0, 0);
		++mStats.drawCalls;
		mStats.triangles += count / 3;
	}

	// 縁取りの物を描く（描く先は呼ぶ側が置く）。印を読むときは maskTexture を t0 に置く
	//  1 物ごとの定数は縁の色・幅・点滅（マテリアルの metallic / roughness の場所に入れる）
	void Renderer::DrawOutlines(ID3D12GraphicsCommandList* cmd, bool mask, const GpuTexture* maskTexture, const GpuTexture& shadowMap,
		uint32_t width, uint32_t height)
	{
		auto& device = GraphicsDevice::Get();
		BindMeshPass(cmd, shadowMap, width, height);
		if (maskTexture) { cmd->SetGraphicsRootDescriptorTable(kRootAlbedo, maskTexture->GetGpuSrv()); }
		ComPtr<ID3D12PipelineState>(&psos)[2] = mask ? mOutlineMaskPSO : mOutlinePSO;
		for (const OutlineDrawItem& item : mList.outlines)
		{
			if (!item.model || !item.model->IsReady()) { continue; }
			const auto& subs = item.model->GetSubMeshes();
			if (item.subMesh >= subs.size()) { continue; }
			const SubMesh& sub = subs[item.subMesh];
			bool skinned = sub.skinned && item.palette != 0;
			GpuObjectData obj;
			obj.world = item.world;
			obj.worldInvTranspose = Transpose(Inverse(item.world));
			obj.tint = item.color;
			GpuMaterialData mat;
			mat.metallic = item.width;
			mat.roughness = item.blinkSec;
			cmd->SetPipelineState(psos[skinned ? 1 : 0].Get());
			cmd->SetGraphicsRootConstantBufferView(kRootObject, device.UploadConstants(obj));
			cmd->SetGraphicsRootConstantBufferView(kRootMaterial, device.UploadConstants(mat));
			if (skinned) { cmd->SetGraphicsRootShaderResourceView(kRootPalette, item.palette); }
			D3D12_VERTEX_BUFFER_VIEW vbs[2] = { item.model->GetVertexView(), item.model->GetSkinView() };
			cmd->IASetVertexBuffers(0, skinned ? 2 : 1, vbs);
			cmd->IASetIndexBuffer(&item.model->GetIndexView());
			cmd->DrawIndexedInstanced(sub.indexCount, 1, sub.indexStart, 0, 0);
			++mStats.drawCalls;
		}
	}

	// 描画したものを切り替えながら描く（パイプラインと頂点バッファは変わったときだけ設定する）
	void Renderer::DrawMeshes(ID3D12GraphicsCommandList* cmd, const DrawList& items, MeshPass pass)
	{
		const bool shadow = pass == MeshPass::Shadow;
		auto& device = GraphicsDevice::Get();
		auto& assets = AssetManager::Get();
		ID3D12PipelineState* current = nullptr;
		const Model* currentModel = nullptr;
		bool currentSkinned = false;

		for (const MeshDrawItem* item : items)
		{
			const Model& model = *item->model;
			const auto& subs = model.GetSubMeshes();
			if (item->subMesh >= subs.size()) { continue; }
			const SubMesh& sub = subs[item->subMesh];
			const Material& mat = *item->material;
			bool skinned = sub.skinned && item->palette != 0;

			const ShaderAsset* custom = shadow ? nullptr : mat.GetCustomShader();
			ID3D12PipelineState* pso = GetMeshPipeline(skinned, mat.blend, mat.doubleSided, pass, custom);
			if (!pso) { continue; }
			if (pso != current)
			{
				cmd->SetPipelineState(pso);
				current = pso;
			}

			GpuObjectData obj;
			obj.world = item->world;
			obj.worldInvTranspose = Transpose(Inverse(item->world));
			obj.tint = item->tint;
			if (!shadow && mFrameData.lightsPerObject > 0) { SelectObjectLights(item->worldBounds, obj); }
			cmd->SetGraphicsRootConstantBufferView(kRootObject, device.UploadConstants(obj));

			if (!shadow)
			{
				cmd->SetGraphicsRootConstantBufferView(kRootMaterial, device.UploadConstants(mat.ToGpu()));
				cmd->SetGraphicsRootDescriptorTable(kRootAlbedo, TextureSrv(mat.albedo.get(), assets.GetWhiteTexture()));
				cmd->SetGraphicsRootDescriptorTable(kRootNormal, TextureSrv(mat.normal.get(), assets.GetFlatNormalTexture()));
				cmd->SetGraphicsRootDescriptorTable(kRootEmissive, TextureSrv(mat.emissiveMap.get(), assets.GetWhiteTexture()));
				if (custom) { BindShaderProperties(cmd, mat, *custom); }
			}
			if (skinned) { cmd->SetGraphicsRootShaderResourceView(kRootPalette, item->palette); }

			if (&model != currentModel || skinned != currentSkinned)
			{
				D3D12_VERTEX_BUFFER_VIEW vbs[2] = { model.GetVertexView(), model.GetSkinView() };
				cmd->IASetVertexBuffers(0, skinned ? 2 : 1, vbs);
				cmd->IASetIndexBuffer(&model.GetIndexView());
				currentModel = &model;
				currentSkinned = skinned;
			}
			cmd->DrawIndexedInstanced(sub.indexCount, 1, sub.indexStart, 0, 0);
			++mStats.drawCalls;
			mStats.triangles += sub.indexCount / 3;
		}
	}

	// 自作のシェーダーの値を b4 と t5～t8 に置く
	//  値が 1 つもなくても、シェーダーが参照しない枠に何かを置いておく（空のままだと GPU の検証で怒られる）
	//  テクスチャがまだなければ、既定値の名前の色（white / black / gray / bump）で代わりにする
	void Renderer::BindShaderProperties(ID3D12GraphicsCommandList* cmd, const Material& mat, const ShaderAsset& shader)
	{
		auto& device = GraphicsDevice::Get();
		auto& assets = AssetManager::Get();
		mat.BuildShaderConstants(mPropertyScratch);
		if (mPropertyScratch.empty()) { mPropertyScratch.push_back(Vector4()); }
		uint64_t size = sizeof(Vector4) * mPropertyScratch.size();
		cmd->SetGraphicsRootConstantBufferView(kRootShaderProps, device.UploadData(mPropertyScratch.data(), size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT));
		for (int slot = 0; slot < ShaderAsset::kMaxTextures; ++slot)
		{
			const Texture* tex = mat.GetShaderTexture(slot);
			AssetRef<Texture> fallback = assets.GetWhiteTexture();
			for (const ShaderProperty& p : shader.GetProperties())
			{
				if (p.textureSlot == slot) { fallback = assets.GetBuiltinTexture(p.defaultTexture); break; }
			}
			cmd->SetGraphicsRootDescriptorTable(UINT(kRootShaderTex0 + slot), TextureSrv(tex, fallback));
		}
	}

	// 物の境界の箱にいちばん近い所で、明るさ x 減衰が大きい順に Lights Per Object 個まで選ぶ（スポットライトは向きを見ない）
	void Renderer::SelectObjectLights(const AABB& bounds, GpuObjectData& obj) const
	{
		struct Candidate
		{
			float score;
			uint32_t id;
		};
		const uint32_t limit = mFrameData.lightsPerObject;
		Candidate best[kMaxObjectLights];
		uint32_t count = 0;
		auto consider = [&](const Vector3& position, float range, const Vector3& color, float intensity, uint32_t id)
		{
			float dist = Length(Min(Max(position, bounds.min), bounds.max) - position);
			if (dist >= range) { return; }
			float falloff = std::clamp(1.0f - std::pow(dist / range, 4.0f), 0.0f, 1.0f);
			float luminance = color.x * 0.2126f + color.y * 0.7152f + color.z * 0.0722f;
			float score = luminance * intensity * falloff * falloff / (dist * dist + 1.0f);
			if (score <= 0.0f) { return; }
			// 大きい順に並べたまま差し込む（あふれたら一番小さいものを捨てる）
			uint32_t at = count;
			while (at > 0 && best[at - 1].score < score) { --at; }
			if (at >= limit) { return; }
			for (uint32_t i = std::min(count, limit - 1); i > at; --i) { best[i] = best[i - 1]; }
			best[at] = { score, id };
			count = std::min(count + 1, limit);
		};
		for (uint32_t i = 0; i < mFrameData.pointLightCount; ++i)
		{
			const GpuPointLight& l = mList.pointLights[i];
			consider(l.position, l.range, l.color, l.intensity, i);
		}
		for (uint32_t i = 0; i < mFrameData.spotLightCount; ++i)
		{
			const GpuSpotLight& l = mList.spotLights[i];
			consider(l.position, l.range, l.color, l.intensity, i | kObjectLightSpot);
		}
		for (uint32_t i = 0; i < count; ++i) { obj.lights[i] = best[i].id; }
		obj.lightCount = count;
	}

	void Renderer::DrawParticles(ID3D12GraphicsCommandList* cmd)
	{
		if (mList.particles.empty()) { return; }
		auto& device = GraphicsDevice::Get();
		auto& assets = AssetManager::Get();
		Vector3 camPos = mFrameData.cameraPos;
		std::sort(mList.particles.begin(), mList.particles.end(), [&](auto& a, auto& b)
			{
				return LengthSq(a.center - camPos) > LengthSq(b.center - camPos);
			});
		for (auto& p : mList.particles)
		{
			if (p.count == 0) { continue; }
			cmd->SetPipelineState(p.blend == BlendMode::Additive ? mParticleAddPSO.Get() : mParticleAlphaPSO.Get());
			GpuMaterialData mat;
			mat.baseColor = p.color;
			mat.emissive = ToVector3(p.emissive);
			cmd->SetGraphicsRootConstantBufferView(kRootMaterial, device.UploadConstants(mat));
			cmd->SetGraphicsRootDescriptorTable(kRootAlbedo, TextureSrv(p.texture, assets.GetWhiteTexture()));
			cmd->SetGraphicsRootShaderResourceView(kRootPalette, p.instances);
			cmd->DrawInstanced(6, p.count, 0, 0);
			++mStats.drawCalls;
			mStats.particles += p.count;
		}
	}
}
