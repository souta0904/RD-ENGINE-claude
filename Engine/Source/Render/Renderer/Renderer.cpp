#include "RdPch.h"
#include "Render/Renderer/Renderer.h"
#include "Asset/AssetManager.h"
#include "Asset/Model/Model.h"
#include "Math/Geometry/Frustum.h"
#include "Render/Features/BloomFeature.h"
#include "Render/Features/DeferredFeature.h"
#include "Render/Features/ForwardFeature.h"
#include "Render/Features/MeshDrawer.h"
#include "Render/Features/OutlineFeature.h"
#include "Render/Features/OverlayFeature.h"
#include "Render/Features/ScreenshotFeature.h"
#include "Render/Features/ShadowFeature.h"
#include "Render/Features/TonemapFeature.h"
#include "Render/Features/TransparentDrawer.h"
#include "Render/Material/ShaderAsset.h"
#include "Render/Renderer/FrameContext.h"
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

	Renderer::Renderer()
	{
		mMeshes = std::make_unique<MeshDrawer>();
		mTransparent = std::make_unique<TransparentDrawer>(*mMeshes);
		mShadow = std::make_unique<ShadowFeature>(*mMeshes);
		mForward = std::make_unique<ForwardFeature>(*mMeshes, *mTransparent);
		mDeferred = std::make_unique<DeferredFeature>(*mMeshes, *mTransparent);
		mOutline = std::make_unique<OutlineFeature>();
		mBloom = std::make_unique<BloomFeature>();
		mTonemap = std::make_unique<TonemapFeature>();
		mOverlay = std::make_unique<OverlayFeature>();
		mScreenshot = std::make_unique<ScreenshotFeature>();
		mFeatures = { mShadow.get(), mForward.get(), mDeferred.get(), mOutline.get(), mBloom.get(), mTonemap.get(), mOverlay.get(), mScreenshot.get() };
	}

	Renderer::~Renderer() = default;

	void Renderer::Initialize()
	{
		RD_ASSERT(!sInstance);
		sInstance = this;
		mShared.Create();
		CreatePipelines();
		mShadow->Initialize(mSettings.shadowMapSize);
	}

	void Renderer::Shutdown()
	{
		mMeshes->Reset(mShared);
		sInstance = nullptr;
	}

	void Renderer::CreatePipelines()
	{
		mMeshes->Reset(mShared);
		mTransparent->CreatePipelines(mShared);
		for (RenderFeature* feature : mFeatures) { feature->CreatePipelines(mShared); }
		mBlitPSO = mShared.BuildPostPipeline("BlitPS", GraphicsDevice::kBackBufferFormat, BlendMode::Opaque, L"BlitPSO");
	}

	// 自作のシェーダーは Properties も読み直す（足した値がマテリアルの画面に出るように）
	void Renderer::ReloadShaders()
	{
		GraphicsDevice::Get().WaitIdle();
		GraphicsDevice::Get().GetShaderCompiler().ClearCache();
		mMeshes->Reset(mShared);
		for (const auto& asset : AssetManager::Get().GetAllAssets())
		{
			if (auto shader = std::dynamic_pointer_cast<ShaderAsset>(asset)) { shader->Reload(); }
		}
		CreatePipelines();
		Logger::Info(LogCat::Render, "Shaders reloaded");
	}

	void Renderer::RequestCapture(const std::string& path)
	{
		mScreenshot->Request(path);
	}

	void Renderer::FinishCapture()
	{
		mScreenshot->Finish();
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
		mStats = {};
		width = std::max(1u, width);
		height = std::max(1u, height);
		EnsureTargets(width, height);
		mSettings.shadowMapSize = std::clamp(mSettings.shadowMapSize, 256u, 4096u);
		mSettings.shadowCascades = std::clamp(mSettings.shadowCascades, 1u, kMaxShadowCascades);

		// 1. フレームの値を作って GPU へ送る（影の行列はライトが決まってから）
		FrameContext frame{ mShared, camera, env, mSettings, mList, mDebug, mStats };
		frame.width = width;
		frame.height = height;
		frame.isMain = mT == &mMain;
		SetupFrameData(frame, time);
		GpuLightData lights = SetupLights(frame);
		frame.shadowActive = frame.data.dirLightCount > 0 && lights.dirLights[0].castShadow;
		if (frame.shadowActive) { mShadow->Setup(camera, Normalize(lights.dirLights[0].direction), mSettings, frame.data); }
		frame.frameCB = device.UploadConstants(frame.data);
		frame.lightCB = device.UploadConstants(lights);

		// 2. 描くものを集める
		mShadow->CollectCasters(frame);
		CullAndSort(frame);

		// 3. 機能にパスを登録させ、飛ばすものと使い回すものを決めてから実行する（パスの中身は Execute の間に呼ばれる）
		mGraph.Reset();
		constexpr auto kShaderResource = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		frame.textures.output = mGraph.Import("Output", &mT->output, kShaderResource, true);
		frame.textures.shadow = mGraph.Import("Shadow Map", &mShadow->GetShadowMap(mSettings), kShaderResource, false, true);
		rg::TextureDesc depthDesc;
		depthDesc.width = width;
		depthDesc.height = height;
		depthDesc.depth = true;
		frame.textures.depth = mGraph.Create("Depth", depthDesc);
		frame.textures.hdr = mGraph.Create("HDR", { width, height, kHdrFormat, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, false, Color(0, 0, 0, 1) });

		const bool deferred = mSettings.path == RenderPath::Deferred && mDeferred->IsAvailable();
		RenderFeature* opaque = deferred ? static_cast<RenderFeature*>(mDeferred.get()) : mForward.get();
		RenderFeature* order[] = { mShadow.get(), opaque, mOutline.get(), mBloom.get(), mTonemap.get(), mOverlay.get(), mScreenshot.get() };
		for (RenderFeature* feature : order) { feature->AddPasses(mGraph, frame); }

		mGraph.Compile();
		if (frame.isMain) { mGraphInfo = mGraph.GetInfo(); }
		mGraph.Execute(device.GetCommandList());

		mList.Clear();
		mDebug.Clear();
	}

	// 描く先・描画リスト・デバッグ線・統計をメインの分と入れ替えて描き、元に戻す（スクリーンショットはメインの画面だけ）
	void Renderer::RenderOffscreen(RenderTargets& targets, RenderList& list, const RenderCamera& camera, const Environment& env,
		uint32_t width, uint32_t height, float time, DebugDraw* debug)
	{
		RD_GPU_SCOPE("Render Offscreen");
		RenderStats stats = mStats;
		DebugDraw empty;
		DebugDraw& lines = debug ? *debug : empty;
		std::swap(mDebug, lines);
		std::swap(mList, list);
		mT = &targets;

		Render(camera, env, width, height, time);

		mT = &mMain;
		std::swap(mList, list);
		std::swap(mDebug, lines);
		mStats = stats;
	}

	void Renderer::SetupFrameData(FrameContext& frame, float time)
	{
		const RenderCamera& camera = frame.camera;
		const Environment& env = frame.env;
		GpuFrameData& f = frame.data;
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
		f.screenSize = Vector2(float(frame.width), float(frame.height));
		f.shadowBias = mSettings.shadowBias;
		f.shadowStrength = mSettings.shadowStrength;
	}

	// 影を落とせるのは先頭の平行光源 1 つだけなので、影を落とすものを先頭へ並べ替える
	GpuLightData Renderer::SetupLights(FrameContext& frame)
	{
		GpuFrameData& f = frame.data;
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

	void Renderer::CullAndSort(FrameContext& frame)
	{
		RD_PROFILE_SCOPE("Cull & Sort");
		Frustum frustum = Frustum::FromViewProj(frame.data.viewProj);
		for (auto& item : mList.meshes)
		{
			if (!item.model || !item.model->IsReady() || !item.material) { continue; }
			if (mSettings.frustumCulling && !frustum.Intersects(item.worldBounds))
			{
				++mStats.meshesCulled;
				continue;
			}
			item.sortKey = LengthSq(item.worldBounds.GetCenter() - frame.camera.position);
			(item.material->blend == BlendMode::Opaque ? frame.opaque : frame.transparent).push_back(&item);
		}
		std::sort(frame.opaque.begin(), frame.opaque.end(), OpaqueOrder);
		std::sort(frame.transparent.begin(), frame.transparent.end(), TransparentOrder);
		mStats.meshesVisible = uint32_t(frame.opaque.size() + frame.transparent.size());
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
		cmd->SetGraphicsRootSignature(mShared.GetSimpleRS());
		if (mT->output.GetResource()) { mShared.DrawFullscreen(cmd, mBlitPSO.Get(), Vector4(), mT->output.GetGpuSrv(), mStats); }
	}
}
