#pragma once
#include "Math/Color.h"
#include "RHI/Texture/GpuTexture.h"

namespace rd
{
	// レンダーターゲットの作成設定（format は TYPELESS も可。rtv / srv が UNKNOWN なら format と同じ）
	struct RenderTextureDesc
	{
		uint32_t width = 0;
		uint32_t height = 0;
		DXGI_FORMAT format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		DXGI_FORMAT rtvFormat = DXGI_FORMAT_UNKNOWN;
		DXGI_FORMAT srvFormat = DXGI_FORMAT_UNKNOWN;
		Color clearColor = Color::kClear;
		const wchar_t* name = L"RenderTexture";
	};

	// カラーのレンダーターゲット
	class RD_API RenderTexture : public GpuTexture
	{
	public:
		~RenderTexture();
		void Create(const RenderTextureDesc& desc);
		void Resize(uint32_t width, uint32_t height);

		D3D12_CPU_DESCRIPTOR_HANDLE GetRtv() const { return mRtv.cpu; }
		const Color& GetClearColor() const { return mDesc.clearColor; }
		DXGI_FORMAT GetRtvFormat() const { return mDesc.rtvFormat; }
		void Clear(ID3D12GraphicsCommandList* cmd) const { cmd->ClearRenderTargetView(mRtv.cpu, mDesc.clearColor.Data(), 0, nullptr); }

	private:
		RenderTextureDesc mDesc;
		DescriptorHandle mRtv;
	};
}
