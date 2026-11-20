#pragma once
#include "RHI/Descriptor/DescriptorHandle.h"

namespace rd
{
	// SRV を持つ GPU のテクスチャ（RenderTexture / DepthTexture の基底）
	class RD_API GpuTexture
	{
	public:
		GpuTexture() = default;
		~GpuTexture();
		GpuTexture(const GpuTexture&) = delete;
		GpuTexture& operator=(const GpuTexture&) = delete;

		void InitFromResource(ComPtr<ID3D12Resource> resource, DXGI_FORMAT srvFormat, D3D12_RESOURCE_STATES state);
		// 1x1 の単色テクスチャ（rgba は 0xAABBGGRR）
		void CreateSolidColor(uint32_t rgba);
		void Release();

		ID3D12Resource* GetResource() const { return mResource.Get(); }
		const DescriptorHandle& GetSrv() const { return mSrv; }
		D3D12_GPU_DESCRIPTOR_HANDLE GetGpuSrv() const { return mSrv.gpu; }
		uint32_t GetWidth() const { return mWidth; }
		uint32_t GetHeight() const { return mHeight; }
		D3D12_RESOURCE_STATES GetState() const { return mState; }
		void Transition(ID3D12GraphicsCommandList* cmd, D3D12_RESOURCE_STATES state)
		{
			TransitionBarrier(cmd, mResource.Get(), mState, state);
			mState = state;
		}

	protected:
		ComPtr<ID3D12Resource> mResource;
		DescriptorHandle mSrv;
		uint32_t mWidth = 0;
		uint32_t mHeight = 0;
		D3D12_RESOURCE_STATES mState = D3D12_RESOURCE_STATE_COMMON;
	};
}
