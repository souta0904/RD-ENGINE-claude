#pragma once
#include "RHI/Texture/GpuTexture.h"

namespace rd
{
	// 深度バッファ（シェーダーから SRV として読める）
	class DepthTexture : public GpuTexture
	{
	public:
		~DepthTexture();
		void Create(uint32_t width, uint32_t height, const wchar_t* name = L"Depth");
		void Resize(uint32_t width, uint32_t height);

		D3D12_CPU_DESCRIPTOR_HANDLE GetDsv() const { return mDsv.cpu; }
		void Clear(ID3D12GraphicsCommandList* cmd) const
		{
			cmd->ClearDepthStencilView(mDsv.cpu, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
		}

		static constexpr DXGI_FORMAT kResourceFormat = DXGI_FORMAT_R32_TYPELESS;
		static constexpr DXGI_FORMAT kDsvFormat = DXGI_FORMAT_D32_FLOAT;
		static constexpr DXGI_FORMAT kSrvFormat = DXGI_FORMAT_R32_FLOAT;

	private:
		DescriptorHandle mDsv;
		const wchar_t* mName = L"Depth";
	};
}
