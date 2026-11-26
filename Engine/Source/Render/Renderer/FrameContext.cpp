#include "RdPch.h"
#include "Render/Renderer/FrameContext.h"
#include "Render/Renderer/RenderShared.h"
#include "Render/Renderer/RendererCommon.h"

namespace rd
{
	using namespace render;

	void FrameContext::BindMeshPass(ID3D12GraphicsCommandList* cmd, const GpuTexture& shadowMap) const
	{
		SetViewport(cmd, width, height);
		cmd->SetGraphicsRootSignature(shared.GetMeshRS());
		cmd->SetGraphicsRootConstantBufferView(kRootFrame, frameCB);
		cmd->SetGraphicsRootConstantBufferView(kRootLights, lightCB);
		cmd->SetGraphicsRootDescriptorTable(kRootShadow, shadowMap.GetGpuSrv());
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}
}
