#include "RdPch.h"
#include "Render/Features/MeshDrawer.h"
#include "Asset/AssetManager.h"
#include "Asset/Model/Model.h"
#include "Render/Material/ShaderAsset.h"
#include "Render/Renderer/FrameContext.h"
#include "Render/Renderer/RenderShared.h"
#include "Render/Renderer/RendererCommon.h"
#include "RHI/Pipeline/PipelineBuilder.h"

namespace rd
{
	using namespace render;

	void MeshDrawer::Reset(const RenderShared& shared)
	{
		mShared = &shared;
		mPipelines.clear();
	}

	// 描画したものを切り替えながら描く（パイプラインと頂点バッファは変わったときだけ設定する）
	void MeshDrawer::Draw(ID3D12GraphicsCommandList* cmd, const std::vector<const MeshDrawItem*>& items, Pass pass, FrameContext& frame)
	{
		const bool shadow = pass == Pass::Shadow;
		const bool wireframe = frame.settings.wireframe && !shadow;
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
			ID3D12PipelineState* pso = GetPipeline(skinned, mat.blend, mat.doubleSided, pass, wireframe, custom);
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
			if (!shadow && frame.data.lightsPerObject > 0) { SelectObjectLights(item->worldBounds, obj, frame); }
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
			++frame.stats.drawCalls;
			frame.stats.triangles += sub.indexCount / 3;
		}
	}

	// 組み合わせごとに初めて使うときに作ってキャッシュする（キーはビットで組み合わせを表す。上位は自作のシェーダーの番号）
	//  自作のシェーダーがコンパイルできなければ、標準のシェーダーのパイプラインを代わりに覚えておく
	//  （毎フレームコンパイルし直さないため。直したら Reload Shaders で作り直す）
	//  G バッファーに書くものは、同じシェーダーを GBUFFER を定義してコンパイルする（Surface.hlsli の PSMain が切り替わる）
	ID3D12PipelineState* MeshDrawer::GetPipeline(bool skinned, BlendMode blend, bool doubleSided, Pass pass, bool wireframe, const ShaderAsset* custom)
	{
		const bool shadow = pass == Pass::Shadow;
		const bool gbuffer = pass == Pass::GBuffer;
		bool wire = wireframe && !shadow;
		if (shadow) { custom = nullptr; } // 影は形だけなので、どのシェーダーでも同じものを使う
		uint64_t key = (skinned ? 1u : 0u) | (uint32_t(blend) << 1) | (doubleSided ? 16u : 0u) | (shadow ? 32u : 0u) | (wire ? 64u : 0u) |
			(gbuffer ? 128u : 0u);
		if (custom) { key |= uint64_t(custom->GetId()) << 8; }
		if (auto it = mPipelines.find(key); it != mPipelines.end()) { return it->second.Get(); }

		std::vector<std::string> defines;
		if (skinned) { defines.push_back("SKINNED=1"); }
		if (gbuffer) { defines.push_back("GBUFFER=1"); }
		const Shader* vs = nullptr;
		const Shader* ps = nullptr;
		if (custom)
		{
			std::string source = custom->GetPrelude() + custom->GetSource();
			vs = CompileShaderSource(custom->GetPath(), source, "VSMain", ShaderStage::Vertex, defines);
			ps = CompileShaderSource(custom->GetPath(), source, "PSMain", ShaderStage::Pixel, defines);
			if (!vs || !vs->IsValid() || !ps || !ps->IsValid())
			{
				Logger::Error(LogCat::Render, "Shader '{}' failed to compile; drawing with the standard shader instead", custom->GetPath());
				ID3D12PipelineState* fallback = GetPipeline(skinned, blend, doubleSided, pass, wireframe, nullptr);
				mPipelines[key] = fallback;
				return fallback;
			}
		}
		else if (!shadow)
		{
			vs = CompileShader("Mesh.hlsl", "VSMain", ShaderStage::Vertex, defines);
			ps = CompileShader("Mesh.hlsl", "PSMain", ShaderStage::Pixel, defines);
		}
		PipelineBuilder builder;
		builder.SetRootSignature(mShared->GetMeshRS())
			.SetInputLayout(MeshLayout(skinned))
			.SetCull(doubleSided ? CullMode::None : CullMode::Back);

		if (shadow)
		{
			builder.SetVS(CompileShader("Shadow.hlsl", "VSMain", ShaderStage::Vertex, defines))
				.SetDepthBias(1000, 1.5f, 0.01f)
				.SetRenderTargets({}, DepthTexture::kDsvFormat);
			builder.Desc().NumRenderTargets = 0;
		}
		else if (gbuffer)
		{
			builder.SetVS(vs)
				.SetPS(ps)
				.SetBlend(BlendMode::Opaque)
				.SetDepth(DepthMode::ReadWrite)
				.SetWireframe(wire)
				.SetRenderTargets({ kGBufferFormat, kGBufferFormat, kGBufferFormat }, DepthTexture::kDsvFormat);
		}
		else
		{
			builder.SetVS(vs)
				.SetPS(ps)
				.SetBlend(blend)
				.SetDepth(blend == BlendMode::Opaque ? DepthMode::ReadWrite : DepthMode::ReadOnly)
				.SetWireframe(wire)
				.SetRenderTargets({ kHdrFormat }, DepthTexture::kDsvFormat);
		}
		auto pso = builder.Build(GraphicsDevice::Get().GetDevice(), L"MeshPSO");
		mPipelines[key] = pso;
		return pso.Get();
	}

	// 自作のシェーダーの値を b4 と t5～t8 に置く
	//  値が 1 つもなくても、シェーダーが参照しない枠に何かを置いておく（空のままだと GPU の検証で怒られる）
	//  テクスチャがまだなければ、既定値の名前の色（white / black / gray / bump）で代わりにする
	void MeshDrawer::BindShaderProperties(ID3D12GraphicsCommandList* cmd, const Material& mat, const ShaderAsset& shader)
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
	void MeshDrawer::SelectObjectLights(const AABB& bounds, GpuObjectData& obj, const FrameContext& frame)
	{
		struct Candidate
		{
			float score;
			uint32_t id;
		};
		const uint32_t limit = frame.data.lightsPerObject;
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
		for (uint32_t i = 0; i < frame.data.pointLightCount; ++i)
		{
			const GpuPointLight& l = frame.list.pointLights[i];
			consider(l.position, l.range, l.color, l.intensity, i);
		}
		for (uint32_t i = 0; i < frame.data.spotLightCount; ++i)
		{
			const GpuSpotLight& l = frame.list.spotLights[i];
			consider(l.position, l.range, l.color, l.intensity, i | kObjectLightSpot);
		}
		for (uint32_t i = 0; i < count; ++i) { obj.lights[i] = best[i].id; }
		obj.lightCount = count;
	}
}
