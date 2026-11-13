#include "RdPch.h"
#include "Render/Renderer.h"
#include "Asset/AssetManager.h"
#include "Asset/Model.h"
#include "Asset/Texture.h"
#include "Core/StringUtil.h"
#include "RHI/GraphicsDevice.h"
#include <DirectXTex.h>

namespace rd
{
	namespace
	{
		constexpr DXGI_FORMAT kHdrFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
		constexpr DXGI_FORMAT kBloomFormat = DXGI_FORMAT_R11G11B10_FLOAT;
		constexpr DXGI_FORMAT kOutputRtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		constexpr const char* kShaderDir = "Assets/Engine/Shaders/";

		// メッシュ用ルートパラメータ番号
		enum MeshRootParam : UINT
		{
			kRootObject = 0,
			kRootFrame,
			kRootMaterial,
			kRootLights,
			kRootAlbedo,
			kRootNormal,
			kRootPalette,
			kRootShadow,
		};
		// Simple 用
		enum SimpleRootParam : UINT
		{
			kSimpleConstants = 0,
			kSimpleTex0,
			kSimpleTex1,
		};

		struct SimpleConstants
		{
			Matrix4 matrix;
			Vector4 params;
		};

		const D3D12_INPUT_ELEMENT_DESC kStaticLayout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		const D3D12_INPUT_ELEMENT_DESC kSkinnedLayout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		const D3D12_INPUT_ELEMENT_DESC kLineLayout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		struct SpriteVertex
		{
			Vector2 position;
			Vector2 uv;
			Color color;
		};
		const D3D12_INPUT_ELEMENT_DESC kSpriteLayout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		const Shader* CompileShader(const char* file, const char* entry, ShaderStage stage, std::vector<std::string> defines = {})
		{
			return GraphicsDevice::Get().GetShaderCompiler().Compile({ std::string(kShaderDir) + file, entry, stage, std::move(defines) });
		}

		D3D12_GPU_DESCRIPTOR_HANDLE TextureSrv(const Texture* tex, const AssetRef<Texture>& fallback)
		{
			return (tex && tex->IsReady()) ? tex->GetSrv() : fallback->GetSrv();
		}
	}

	// ==================================================
	// 初期化
	// ==================================================
	void Renderer::Initialize()
	{
		RD_ASSERT(!sInstance);
		sInstance = this;
		CreateRootSignatures();
		CreatePipelines();
		mShadowMap.Create(mSettings.shadowMapSize, mSettings.shadowMapSize, L"ShadowMap");
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

	void Renderer::CreateRootSignatures()
	{
		auto device = GraphicsDevice::Get().GetDevice();
		mMeshRS = RootSignatureBuilder()
			.AddCbv(0)// Object
			.AddCbv(1)// Frame
			.AddCbv(2)// Material
			.AddCbv(3)// Lights
			.AddSrvTable(0)// Albedo
			.AddSrvTable(1)// Normal
			.AddSrv(2, D3D12_SHADER_VISIBILITY_VERTEX)// Palette / Particles
			.AddSrvTable(3)// ShadowMap
			.AddStaticSampler(Samplers::AnisotropicWrap(0))
			.AddStaticSampler(Samplers::LinearClamp(1))
			.AddStaticSampler(Samplers::ShadowCompare(2))
			.Build(device, L"MeshRS");

		mSimpleRS = RootSignatureBuilder()
			.AddConstants(0, sizeof(SimpleConstants) / 4)
			.AddSrvTable(0)
			.AddSrvTable(1)
			.AddStaticSampler(Samplers::LinearClamp(0))
			.AddStaticSampler(Samplers::PointClamp(1))
			.AddStaticSampler(Samplers::LinearWrap(2))
			.Build(device, L"SimpleRS");
	}

	void Renderer::CreatePipelines()
	{
		auto device = GraphicsDevice::Get().GetDevice();

		// 空
		mSkyPSO = PipelineBuilder()
			.SetRootSignature(mMeshRS.Get())
			.SetVS(CompileShader("Sky.hlsl", "VSMain", ShaderStage::Vertex))
			.SetPS(CompileShader("Sky.hlsl", "PSMain", ShaderStage::Pixel))
			.SetCull(CullMode::None)
			.SetDepth(DepthMode::ReadOnly, D3D12_COMPARISON_FUNC_LESS_EQUAL)
			.SetRenderTargets({ kHdrFormat }, DepthTexture::kDsvFormat)
			.Build(device, L"SkyPSO");

		// パーティクル
		auto particle = PipelineBuilder()
			.SetRootSignature(mMeshRS.Get())
			.SetVS(CompileShader("Particle.hlsl", "VSMain", ShaderStage::Vertex))
			.SetPS(CompileShader("Particle.hlsl", "PSMain", ShaderStage::Pixel))
			.SetCull(CullMode::None)
			.SetDepth(DepthMode::ReadOnly)
			.SetRenderTargets({ kHdrFormat }, DepthTexture::kDsvFormat);
		mParticleAlphaPSO = particle.SetBlend(BlendMode::Alpha).Build(device, L"ParticleAlphaPSO");
		mParticleAddPSO = particle.SetBlend(BlendMode::Additive).Build(device, L"ParticleAddPSO");

		// デバッグ線
		auto line = PipelineBuilder()
			.SetRootSignature(mSimpleRS.Get())
			.SetVS(CompileShader("Simple.hlsl", "LineVS", ShaderStage::Vertex))
			.SetPS(CompileShader("Simple.hlsl", "LinePS", ShaderStage::Pixel))
			.SetInputLayout(kLineLayout)
			.SetTopology(D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE)
			.SetBlend(BlendMode::Alpha)
			.SetCull(CullMode::None)
			.SetRenderTargets({ kOutputRtvFormat }, DepthTexture::kDsvFormat);
		mLineDepthPSO = line.SetDepth(DepthMode::ReadOnly).Build(device, L"LineDepthPSO");
		mLineOverlayPSO = line.SetDepth(DepthMode::Disabled).Build(device, L"LineOverlayPSO");

		// スプライト
		mSpritePSO = PipelineBuilder()
			.SetRootSignature(mSimpleRS.Get())
			.SetVS(CompileShader("Simple.hlsl", "SpriteVS", ShaderStage::Vertex))
			.SetPS(CompileShader("Simple.hlsl", "SpritePS", ShaderStage::Pixel))
			.SetInputLayout(kSpriteLayout)
			.SetBlend(BlendMode::Alpha)
			.SetCull(CullMode::None)
			.SetDepth(DepthMode::Disabled)
			.SetRenderTargets({ kOutputRtvFormat }, DepthTexture::kDsvFormat)
			.Build(device, L"SpritePSO");

		// ポストエフェクト
		const Shader* fullscreen = CompileShader("Post.hlsl", "FullscreenVS", ShaderStage::Vertex);
		auto post = [&](const char* ps, DXGI_FORMAT format, BlendMode blend, const wchar_t* name)
			{
				return PipelineBuilder()
					.SetRootSignature(mSimpleRS.Get())
					.SetVS(fullscreen)
					.SetPS(CompileShader("Post.hlsl", ps, ShaderStage::Pixel))
					.SetBlend(blend)
					.SetCull(CullMode::None)
					.SetDepth(DepthMode::Disabled)
					.SetRenderTargets({ format }, DXGI_FORMAT_UNKNOWN)
					.Build(device, name);
			};
		mBloomPrefilterPSO = post("BloomPrefilterPS", kBloomFormat, BlendMode::Opaque, L"BloomPrefilterPSO");
		mBloomDownPSO = post("BloomDownPS", kBloomFormat, BlendMode::Opaque, L"BloomDownPSO");
		mBloomUpPSO = post("BloomUpPS", kBloomFormat, BlendMode::Additive, L"BloomUpPSO");
		mTonemapPSO = post("TonemapPS", kOutputRtvFormat, BlendMode::Opaque, L"TonemapPSO");
		mBlitPSO = post("BlitPS", GraphicsDevice::kBackBufferFormat, BlendMode::Opaque, L"BlitPSO");
	}

	ID3D12PipelineState* Renderer::GetMeshPipeline(bool skinned, BlendMode blend, bool doubleSided, bool shadow)
	{
		bool wire = mSettings.wireframe && !shadow;
		uint32_t key = (skinned ? 1u : 0u) | (uint32_t(blend) << 1) | (doubleSided ? 16u : 0u) | (shadow ? 32u : 0u) | (wire ? 64u : 0u);
		if (auto it = mMeshPipelines.find(key); it != mMeshPipelines.end()) { return it->second.Get(); }

		std::vector<std::string> defines;
		if (skinned) { defines.push_back("SKINNED=1"); }
		PipelineBuilder builder;
		builder.SetRootSignature(mMeshRS.Get())
			.SetInputLayout(skinned ? std::span<const D3D12_INPUT_ELEMENT_DESC>(kSkinnedLayout) : std::span<const D3D12_INPUT_ELEMENT_DESC>(kStaticLayout))
			.SetCull(doubleSided ? CullMode::None : CullMode::Back);

		if (shadow)
		{
			builder.SetVS(CompileShader("Shadow.hlsl", "VSMain", ShaderStage::Vertex, defines))
				.SetDepthBias(1000, 1.5f, 0.01f)
				.SetRenderTargets({}, DepthTexture::kDsvFormat);
			builder.Desc().NumRenderTargets = 0;
		}
		else
		{
			builder.SetVS(CompileShader("Mesh.hlsl", "VSMain", ShaderStage::Vertex, defines))
				.SetPS(CompileShader("Mesh.hlsl", "PSMain", ShaderStage::Pixel, defines))
				.SetBlend(blend)
				.SetDepth(blend == BlendMode::Opaque ? DepthMode::ReadWrite : DepthMode::ReadOnly)
				.SetWireframe(wire)
				.SetRenderTargets({ kHdrFormat }, DepthTexture::kDsvFormat);
		}
		auto pso = builder.Build(GraphicsDevice::Get().GetDevice(), L"MeshPSO");
		mMeshPipelines[key] = pso;
		return pso.Get();
	}

	void Renderer::EnsureTargets(uint32_t width, uint32_t height)
	{
		width = std::max(1u, width);
		height = std::max(1u, height);
		if (mHdr.GetResource() && mHdr.GetWidth() == width && mHdr.GetHeight() == height) { return; }

		mHdr.Create({ width, height, kHdrFormat, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, Color(0, 0, 0, 1), L"HDR" });
		mDepth.Resize(width, height);
		mOutput.Create({ width, height, DXGI_FORMAT_R8G8B8A8_TYPELESS, kOutputRtvFormat, DXGI_FORMAT_R8G8B8A8_UNORM, Color(0, 0, 0, 1), L"Output" });
		uint32_t w = width, h = height;
		for (uint32_t i = 0; i < kBloomLevels; ++i)
		{
			w = std::max(1u, w / 2);
			h = std::max(1u, h / 2);
			mBloom[i].Create({ w, h, kBloomFormat, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, Color(0, 0, 0, 1), L"Bloom" });
		}
	}

	// ==================================================
	// 描画
	// ==================================================
	void Renderer::Render(const RenderCamera& camera, const Environment& env, uint32_t width, uint32_t height, float time)
	{
		auto& device = GraphicsDevice::Get();
		auto cmd = device.GetCommandList();
		mStats = {};
		EnsureTargets(width, height);
		if (mShadowMap.GetWidth() != mSettings.shadowMapSize)
		{
			mShadowMap.Resize(mSettings.shadowMapSize, mSettings.shadowMapSize);
		}

		// ---------------- フレーム定数 ----------------
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
		f.ambientSky = Vector3(env.ambientSky.r, env.ambientSky.g, env.ambientSky.b) * env.ambientIntensity;
		f.ambientGround = Vector3(env.ambientGround.r, env.ambientGround.g, env.ambientGround.b) * env.ambientIntensity;
		f.skyZenith = Vector3(env.skyZenith.r, env.skyZenith.g, env.skyZenith.b);
		f.skyHorizon = Vector3(env.skyHorizon.r, env.skyHorizon.g, env.skyHorizon.b);
		f.fogColor = Vector3(env.fogColor.r, env.fogColor.g, env.fogColor.b);
		f.fogDensity = env.fogDensity;
		f.fogStart = env.fogStart;
		f.screenSize = Vector2(float(width), float(height));
		f.shadowBias = mSettings.shadowBias;
		f.shadowStrength = mSettings.shadowStrength;

		// ---------------- ライト ----------------
		GpuLightData lights = {};
		f.dirLightCount = std::min<uint32_t>(uint32_t(mList.dirLights.size()), kMaxDirLights);
		f.pointLightCount = std::min<uint32_t>(uint32_t(mList.pointLights.size()), kMaxPointLights);
		f.spotLightCount = std::min<uint32_t>(uint32_t(mList.spotLights.size()), kMaxSpotLights);
		// 影を落とす平行光源を先頭へ
		std::stable_sort(mList.dirLights.begin(), mList.dirLights.end(), [](auto& a, auto& b) { return a.castShadow > b.castShadow; });
		for (uint32_t i = 0; i < f.dirLightCount; ++i)
		{
			lights.dirLights[i] = mList.dirLights[i];
			lights.dirLights[i].castShadow = (i == 0 && mList.dirLights[i].castShadow && mSettings.shadows) ? 1u : 0u;
		}
		for (uint32_t i = 0; i < f.pointLightCount; ++i) { lights.pointLights[i] = mList.pointLights[i]; }
		for (uint32_t i = 0; i < f.spotLightCount; ++i) { lights.spotLights[i] = mList.spotLights[i]; }
		mStats.lights = f.dirLightCount + f.pointLightCount + f.spotLightCount;

		// ---------------- シャドウ行列（テクセルにスナップしてちらつきを防ぐ） ----------------
		mShadowActive = f.dirLightCount > 0 && lights.dirLights[0].castShadow;
		if (mShadowActive)
		{
			Vector3 lightDir = Normalize(lights.dirLights[0].direction);
			float radius = mSettings.shadowDistance * 0.5f;
			Vector3 center = camera.position + camera.GetForward() * radius;
			Vector3 up = std::fabs(lightDir.y) > 0.99f ? Vector3::kUnitZ : Vector3::kUnitY;
			Matrix4 lightView = Matrix4::CreateLookAt(Vector3::kZero, lightDir, up);
			Vector3 c = TransformPoint(center, lightView);
			float texel = (radius * 2.0f) / float(mSettings.shadowMapSize);
			c.x = std::floor(c.x / texel) * texel;
			c.y = std::floor(c.y / texel) * texel;
			Matrix4 lightProj = Matrix4::CreateOrthographicOffCenter(c.x - radius, c.x + radius, c.y - radius, c.y + radius,
				c.z - radius * 4.0f, c.z + radius * 4.0f);
			f.shadowViewProj = lightView * lightProj;
			f.shadowTexelSize = texel;
			f.shadowEnabled = 1;
		}

		mFrameCB = device.UploadConstants(f);
		mLightCB = device.UploadConstants(lights);

		// ---------------- カリングと並び替え ----------------
		Frustum frustum = Frustum::FromViewProj(f.viewProj);
		Frustum shadowFrustum = Frustum::FromViewProj(f.shadowViewProj);
		std::vector<const MeshDrawItem*> opaque, transparent, casters;
		for (auto& item : mList.meshes)
		{
			if (!item.model || !item.model->IsReady() || !item.material) { continue; }
			if (mShadowActive && item.material->castShadows && item.material->blend == BlendMode::Opaque &&
				shadowFrustum.Intersects(item.worldBounds))
			{
				casters.push_back(&item);
			}
			if (mSettings.frustumCulling && !frustum.Intersects(item.worldBounds))
			{
				++mStats.meshesCulled;
				continue;
			}
			item.sortKey = LengthSq(item.worldBounds.GetCenter() - camera.position);
			(item.material->blend == BlendMode::Opaque ? opaque : transparent).push_back(&item);
		}
		std::sort(opaque.begin(), opaque.end(), [](auto a, auto b)
			{
				// パイプライン（スキン有無）→ 手前から
				bool sa = a->palette != 0, sb = b->palette != 0;
				return sa != sb ? sa < sb : a->sortKey < b->sortKey;
			});
		std::sort(transparent.begin(), transparent.end(), [](auto a, auto b) { return a->sortKey > b->sortKey; });
		mStats.meshesVisible = uint32_t(opaque.size() + transparent.size());
		mStats.shadowCasters = uint32_t(casters.size());

		// ---------------- シャドウパス ----------------
		if (mShadowActive) { ShadowPass(cmd, casters); }

		// ---------------- メインパス（HDR） ----------------
		mHdr.Transition(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);
		mDepth.Transition(cmd, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		mShadowMap.Transition(cmd, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		auto rtv = mHdr.GetRtv();
		auto dsv = mDepth.GetDsv();
		cmd->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
		Color clear(f.skyHorizon.x, f.skyHorizon.y, f.skyHorizon.z, 1.0f);
		cmd->ClearRenderTargetView(rtv, clear.Data(), 0, nullptr);
		mDepth.Clear(cmd);
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

		// ---------------- ポストエフェクト ----------------
		mHdr.Transition(cmd, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		BloomPass(cmd);
		TonemapPass(cmd);
		OverlayPass(cmd, f.viewProj);

		// スクリーンショット用に読み戻しバッファへコピー
		if (!mCapturePath.empty())
		{
			auto desc = mOutput.GetResource()->GetDesc();
			UINT64 total = 0;
			device.GetDevice()->GetCopyableFootprints(&desc, 0, 1, 0, &mCaptureFootprint, nullptr, nullptr, &total);
			CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_READBACK);
			// total は最終行のパディングを含まないので、読み出し側（RowPitch * Height）に合わせて確保する
			UINT64 size = std::max<UINT64>(total, UINT64(mCaptureFootprint.Footprint.RowPitch) * mCaptureFootprint.Footprint.Height);
			auto bufDesc = CD3DX12_RESOURCE_DESC::Buffer(size);
			mCaptureBuffer.Reset();
			RD_CHECK_HR(device.GetDevice()->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &bufDesc,
				D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&mCaptureBuffer)));
			mOutput.Transition(cmd, D3D12_RESOURCE_STATE_COPY_SOURCE);
			CD3DX12_TEXTURE_COPY_LOCATION dst(mCaptureBuffer.Get(), mCaptureFootprint);
			CD3DX12_TEXTURE_COPY_LOCATION src(mOutput.GetResource(), 0);
			cmd->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
			mPendingCapturePath = std::move(mCapturePath);
			mCapturePath.clear();
		}
		mOutput.Transition(cmd, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		mList.Clear();
		mDebug.Clear();
	}

	void Renderer::ShadowPass(ID3D12GraphicsCommandList* cmd, const std::vector<const MeshDrawItem*>& casters)
	{
		mShadowMap.Transition(cmd, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		auto dsv = mShadowMap.GetDsv();
		cmd->OMSetRenderTargets(0, nullptr, FALSE, &dsv);
		mShadowMap.Clear(cmd);
		SetViewport(cmd, mShadowMap.GetWidth(), mShadowMap.GetHeight());
		cmd->SetGraphicsRootSignature(mMeshRS.Get());
		cmd->SetGraphicsRootConstantBufferView(kRootFrame, mFrameCB);
		cmd->SetGraphicsRootConstantBufferView(kRootLights, mLightCB);
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		DrawMeshes(cmd, casters, true);
	}

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
			mat.emissive = Vector3(p.emissive.r, p.emissive.g, p.emissive.b);
			cmd->SetGraphicsRootConstantBufferView(kRootMaterial, device.UploadConstants(mat));
			cmd->SetGraphicsRootDescriptorTable(kRootAlbedo, TextureSrv(p.texture, assets.GetWhiteTexture()));
			cmd->SetGraphicsRootShaderResourceView(kRootPalette, p.instances);
			cmd->DrawInstanced(6, p.count, 0, 0);
			++mStats.drawCalls;
			mStats.particles += p.count;
		}
	}

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

	void Renderer::BloomPass(ID3D12GraphicsCommandList* cmd)
	{
		cmd->SetGraphicsRootSignature(mSimpleRS.Get());
		if (!mSettings.bloom) { return; }

		auto renderTo = [&](RenderTexture& target, bool clear)
			{
				target.Transition(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);
				auto rtv = target.GetRtv();
				cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
				if (clear) { target.Clear(cmd); }
				SetViewport(cmd, target.GetWidth(), target.GetHeight());
			};

		// 明るい部分の抽出 + 縮小
		renderTo(mBloom[0], false);
		DrawFullscreen(cmd, mBloomPrefilterPSO.Get(), Vector4(mSettings.bloomThreshold, mSettings.bloomKnee, 0, 0), mHdr.GetGpuSrv());
		mBloom[0].Transition(cmd, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		// 段階的に縮小
		for (uint32_t i = 1; i < kBloomLevels; ++i)
		{
			renderTo(mBloom[i], false);
			DrawFullscreen(cmd, mBloomDownPSO.Get(), Vector4(), mBloom[i - 1].GetGpuSrv());
			mBloom[i].Transition(cmd, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}
		// 拡大しながら加算
		for (uint32_t i = kBloomLevels - 1; i > 0; --i)
		{
			renderTo(mBloom[i - 1], false);
			DrawFullscreen(cmd, mBloomUpPSO.Get(), Vector4(1.0f, 0, 0, 0), mBloom[i].GetGpuSrv());
			mBloom[i - 1].Transition(cmd, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}
	}

	void Renderer::TonemapPass(ID3D12GraphicsCommandList* cmd)
	{
		cmd->SetGraphicsRootSignature(mSimpleRS.Get());
		mOutput.Transition(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);
		auto rtv = mOutput.GetRtv();
		cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
		SetViewport(cmd, mOutput.GetWidth(), mOutput.GetHeight());
		Vector4 params(mSettings.exposure, mSettings.bloom ? mSettings.bloomIntensity : 0.0f, mSettings.vignette, mSettings.tonemap ? 1.0f : 0.0f);
		// ブルーム無効時は黒いテクスチャ代わりに HDR を渡し、強度 0 で無視する
		DrawFullscreen(cmd, mTonemapPSO.Get(), params, mHdr.GetGpuSrv(), mSettings.bloom ? mBloom[0].GetGpuSrv() : mHdr.GetGpuSrv());
	}

	void Renderer::OverlayPass(ID3D12GraphicsCommandList* cmd, const Matrix4& viewProj)
	{
		auto rtv = mOutput.GetRtv();
		auto dsv = mDepth.GetDsv();
		cmd->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
		SetViewport(cmd, mOutput.GetWidth(), mOutput.GetHeight());
		cmd->SetGraphicsRootSignature(mSimpleRS.Get());
		DrawLines(cmd, mDebug.GetDepthTested(), mLineDepthPSO.Get(), viewProj);
		DrawLines(cmd, mDebug.GetOverlay(), mLineOverlayPSO.Get(), viewProj);
		DrawSprites(cmd);
	}

	void Renderer::DrawLines(ID3D12GraphicsCommandList* cmd, const std::vector<DebugDraw::Vertex>& lines, ID3D12PipelineState* pso, const Matrix4& viewProj)
	{
		if (lines.empty() || !pso) { return; }
		auto& device = GraphicsDevice::Get();
		uint64_t size = sizeof(DebugDraw::Vertex) * lines.size();
		D3D12_VERTEX_BUFFER_VIEW vbv = { device.UploadData(lines.data(), size), UINT(size), sizeof(DebugDraw::Vertex) };
		SimpleConstants c;
		c.matrix = viewProj;
		cmd->SetPipelineState(pso);
		cmd->SetGraphicsRoot32BitConstants(kSimpleConstants, sizeof(c) / 4, &c, 0);
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
		cmd->IASetVertexBuffers(0, 1, &vbv);
		cmd->DrawInstanced(UINT(lines.size()), 1, 0, 0);
		++mStats.drawCalls;
	}

	void Renderer::DrawSprites(ID3D12GraphicsCommandList* cmd)
	{
		auto& sprites = mList.sprites;
		if (sprites.empty() || !mSpritePSO) { return; }
		auto& device = GraphicsDevice::Get();
		auto& assets = AssetManager::Get();
		std::stable_sort(sprites.begin(), sprites.end(), [](auto& a, auto& b) { return a.layer < b.layer; });

		// 頂点を生成
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
		c.matrix = Matrix4::CreateOrthographic(0.0f, 0.0f, float(mOutput.GetWidth()), float(mOutput.GetHeight()), 0.0f, 1.0f);
		cmd->SetPipelineState(mSpritePSO.Get());
		cmd->SetGraphicsRoot32BitConstants(kSimpleConstants, sizeof(c) / 4, &c, 0);
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmd->IASetVertexBuffers(0, 1, &vbv);

		// 同じテクスチャが続く範囲をまとめて描く
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
			if (SUCCEEDED(hr)) { Log::Info(LogCat::Render, "Saved screenshot: {}", mPendingCapturePath); }
			else { Log::Error(LogCat::Render, "Failed to save screenshot: {}", mPendingCapturePath); }
		}
		mPendingCapturePath.clear();
		mCaptureBuffer.Reset();
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
		if (mOutput.GetResource()) { DrawFullscreen(cmd, mBlitPSO.Get(), Vector4(), mOutput.GetGpuSrv()); }
	}
}
