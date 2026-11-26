#include "RdPch.h"
#include "Render/Features/TransparentDrawer.h"
#include "Asset/AssetManager.h"
#include "Render/Features/MeshDrawer.h"
#include "Render/Renderer/FrameContext.h"
#include "Render/Renderer/RenderShared.h"
#include "Render/Renderer/RendererCommon.h"
#include "RHI/Pipeline/PipelineBuilder.h"

namespace rd
{
	using namespace render;

	namespace
	{
		Vector3 ToVector3(const Color& c) { return { c.r, c.g, c.b }; }
	}

	void TransparentDrawer::CreatePipelines(const RenderShared& shared)
	{
		auto device = GraphicsDevice::Get().GetDevice();
		mSkyPSO = PipelineBuilder()
			.SetRootSignature(shared.GetMeshRS())
			.SetVS(CompileShader("Sky.hlsl", "VSMain", ShaderStage::Vertex))
			.SetPS(CompileShader("Sky.hlsl", "PSMain", ShaderStage::Pixel))
			.SetCull(CullMode::None)
			.SetDepth(DepthMode::ReadOnly, D3D12_COMPARISON_FUNC_LESS_EQUAL)
			.SetRenderTargets({ kHdrFormat }, DepthTexture::kDsvFormat)
			.Build(device, L"SkyPSO");

		PipelineBuilder particle;
		particle.SetRootSignature(shared.GetMeshRS())
			.SetVS(CompileShader("Particle.hlsl", "VSMain", ShaderStage::Vertex))
			.SetPS(CompileShader("Particle.hlsl", "PSMain", ShaderStage::Pixel))
			.SetCull(CullMode::None)
			.SetDepth(DepthMode::ReadOnly)
			.SetRenderTargets({ kHdrFormat }, DepthTexture::kDsvFormat);
		mParticleAlphaPSO = particle.SetBlend(BlendMode::Alpha).Build(device, L"ParticleAlphaPSO");
		mParticleAddPSO = particle.SetBlend(BlendMode::Additive).Build(device, L"ParticleAddPSO");

		// 線と同じシェーダで三角形を描く（HDR に描くのでトーンマップがかかる）
		mTranslucentTrianglePSO = PipelineBuilder()
			.SetRootSignature(shared.GetSimpleRS())
			.SetVS(CompileShader("Simple.hlsl", "LineVS", ShaderStage::Vertex))
			.SetPS(CompileShader("Simple.hlsl", "LinePS", ShaderStage::Pixel))
			.SetInputLayout(kLineLayout)
			.SetBlend(BlendMode::Alpha)
			.SetCull(CullMode::None)
			.SetDepth(DepthMode::ReadOnly)
			.SetRenderTargets({ kHdrFormat }, DepthTexture::kDsvFormat)
			.Build(device, L"TranslucentTrianglePSO");
	}

	void TransparentDrawer::Draw(ID3D12GraphicsCommandList* cmd, FrameContext& frame)
	{
		if (frame.env.drawSky && mSkyPSO)
		{
			cmd->SetPipelineState(mSkyPSO.Get());
			cmd->DrawInstanced(3, 1, 0, 0);
			++frame.stats.drawCalls;
		}
		mMeshes.Draw(cmd, frame.transparent, MeshDrawer::Pass::Forward, frame);
		DrawParticles(cmd, frame);
		DrawTranslucentTriangles(cmd, frame);
	}

	// 奥から順に描く（半透明が重なっても前後が崩れないように）
	void TransparentDrawer::DrawParticles(ID3D12GraphicsCommandList* cmd, FrameContext& frame)
	{
		auto& particles = frame.list.particles;
		if (particles.empty()) { return; }
		auto& device = GraphicsDevice::Get();
		auto& assets = AssetManager::Get();
		Vector3 camPos = frame.data.cameraPos;
		std::sort(particles.begin(), particles.end(), [&](auto& a, auto& b)
			{
				return LengthSq(a.center - camPos) > LengthSq(b.center - camPos);
			});
		for (auto& p : particles)
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
			++frame.stats.drawCalls;
			frame.stats.particles += p.count;
		}
	}

	void TransparentDrawer::DrawTranslucentTriangles(ID3D12GraphicsCommandList* cmd, FrameContext& frame)
	{
		auto& tris = frame.list.translucentTriangles;
		if (tris.size() < 3 || !mTranslucentTrianglePSO) { return; }
		auto& device = GraphicsDevice::Get();
		UINT count = UINT(tris.size() / 3 * 3);
		uint64_t size = sizeof(ColorVertex) * count;
		D3D12_VERTEX_BUFFER_VIEW vbv = { device.UploadData(tris.data(), size), UINT(size), sizeof(ColorVertex) };
		SimpleConstants c;
		c.matrix = frame.data.viewProj;
		cmd->SetGraphicsRootSignature(frame.shared.GetSimpleRS());
		cmd->SetPipelineState(mTranslucentTrianglePSO.Get());
		cmd->SetGraphicsRoot32BitConstants(kSimpleConstants, sizeof(c) / 4, &c, 0);
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmd->IASetVertexBuffers(0, 1, &vbv);
		cmd->DrawInstanced(count, 1, 0, 0);
		++frame.stats.drawCalls;
		frame.stats.triangles += count / 3;
	}
}
