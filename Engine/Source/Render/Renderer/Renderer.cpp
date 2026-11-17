#include "RdPch.h"
#include "Render/Renderer/Renderer.h"
#include "Asset/AssetManager.h"
#include "Asset/Model/Model.h"
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

	void Renderer::ReloadShaders()
	{
		GraphicsDevice::Get().WaitIdle();
		GraphicsDevice::Get().GetShaderCompiler().ClearCache();
		mMeshPipelines.clear();
		CreatePipelines();
		Log::Info(LogCat::Render, "Shaders reloaded");
	}

	void Renderer::EnsureTargets(uint32_t width, uint32_t height)
	{
		width = std::max(1u, width);
		height = std::max(1u, height);
		if (mT->hdr.GetResource() && mT->hdr.GetWidth() == width && mT->hdr.GetHeight() == height) { return; }

		mT->hdr.Create({ width, height, kHdrFormat, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, Color(0, 0, 0, 1), L"HDR" });
		mT->depth.Resize(width, height);
		mT->output.Create({ width, height, DXGI_FORMAT_R8G8B8A8_TYPELESS, kOutputRtvFormat, DXGI_FORMAT_R8G8B8A8_UNORM, Color(0, 0, 0, 1), L"Output" });
		uint32_t w = width, h = height;
		for (uint32_t i = 0; i < RenderTargets::kBloomLevels; ++i)
		{
			w = std::max(1u, w / 2);
			h = std::max(1u, h / 2);
			mT->bloom[i].Create({ w, h, kBloomFormat, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, Color(0, 0, 0, 1), L"Bloom" });
		}
	}

	void Renderer::Render(const RenderCamera& camera, const Environment& env, uint32_t width, uint32_t height, float time)
	{
		RD_GPU_SCOPE("Render Scene");
		auto& device = GraphicsDevice::Get();
		auto cmd = device.GetCommandList();
		mStats = {};
		EnsureTargets(width, height);
		mSettings.shadowMapSize = std::clamp(mSettings.shadowMapSize, 256u, 4096u);
		mSettings.shadowCascades = std::clamp(mSettings.shadowCascades, 1u, kMaxShadowCascades);
		if (mShadowMap.GetWidth() != mSettings.shadowMapSize * 2)
		{
			mShadowMap.Resize(mSettings.shadowMapSize * 2, mSettings.shadowMapSize * 2);
		}

		SetupFrameData(camera, env, width, height, time);
		GpuLightData lights = SetupLights();
		mShadowActive = mFrameData.dirLightCount > 0 && lights.dirLights[0].castShadow;
		if (mShadowActive) { SetupShadowCascades(camera, Normalize(lights.dirLights[0].direction), mFrameData); }
		mFrameCB = device.UploadConstants(mFrameData);
		mLightCB = device.UploadConstants(lights);

		std::vector<const MeshDrawItem*> opaque, transparent;
		CasterLists casters;
		CullAndSort(camera, opaque, transparent, casters);

		if (mShadowActive) { ShadowPass(cmd, casters); }
		MainPass(cmd, env, width, height, opaque, transparent);

		mT->hdr.Transition(cmd, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		BloomPass(cmd);
		TonemapPass(cmd);
		OverlayPass(cmd, mFrameData.viewProj);

		if (!mCapturePath.empty()) { CopyOutputForCapture(cmd); }
		mT->output.Transition(cmd, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		mList.Clear();
		mDebug.Clear();
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
	void Renderer::CullAndSort(const RenderCamera& camera, std::vector<const MeshDrawItem*>& opaque,
		std::vector<const MeshDrawItem*>& transparent, CasterLists& casters)
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

	void Renderer::MainPass(ID3D12GraphicsCommandList* cmd, const Environment& env, uint32_t width, uint32_t height,
		const std::vector<const MeshDrawItem*>& opaque, const std::vector<const MeshDrawItem*>& transparent)
	{
		RD_GPU_SCOPE("Main Pass");
		mT->hdr.Transition(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);
		mT->depth.Transition(cmd, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		mShadowMap.Transition(cmd, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		auto rtv = mT->hdr.GetRtv();
		auto dsv = mT->depth.GetDsv();
		cmd->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
		Color clear(mFrameData.skyHorizon.x, mFrameData.skyHorizon.y, mFrameData.skyHorizon.z, 1.0f);
		cmd->ClearRenderTargetView(rtv, clear.Data(), 0, nullptr);
		mT->depth.Clear(cmd);
		SetViewport(cmd, width, height);

		cmd->SetGraphicsRootSignature(mMeshRS.Get());
		cmd->SetGraphicsRootConstantBufferView(kRootFrame, mFrameCB);
		cmd->SetGraphicsRootConstantBufferView(kRootLights, mLightCB);
		cmd->SetGraphicsRootDescriptorTable(kRootShadow, mShadowMap.GetGpuSrv());
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		DrawMeshes(cmd, opaque, false);

		if (env.drawSky && mSkyPSO)
		{
			cmd->SetPipelineState(mSkyPSO.Get());
			cmd->DrawInstanced(3, 1, 0, 0);
			++mStats.drawCalls;
		}

		DrawMeshes(cmd, transparent, false);
		DrawParticles(cmd);
		DrawTranslucentTriangles(cmd);
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

	// 描画したものを切り替えながら描く（パイプラインと頂点バッファは変わったときだけ設定する）
	void Renderer::DrawMeshes(ID3D12GraphicsCommandList* cmd, const std::vector<const MeshDrawItem*>& items, bool shadow)
	{
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

			ID3D12PipelineState* pso = GetMeshPipeline(skinned, mat.blend, mat.doubleSided, shadow);
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
			cmd->SetGraphicsRootConstantBufferView(kRootObject, device.UploadConstants(obj));

			if (!shadow)
			{
				cmd->SetGraphicsRootConstantBufferView(kRootMaterial, device.UploadConstants(mat.ToGpu()));
				cmd->SetGraphicsRootDescriptorTable(kRootAlbedo, TextureSrv(mat.albedo.get(), assets.GetWhiteTexture()));
				cmd->SetGraphicsRootDescriptorTable(kRootNormal, TextureSrv(mat.normal.get(), assets.GetFlatNormalTexture()));
				cmd->SetGraphicsRootDescriptorTable(kRootEmissive, TextureSrv(mat.emissiveMap.get(), assets.GetWhiteTexture()));
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
