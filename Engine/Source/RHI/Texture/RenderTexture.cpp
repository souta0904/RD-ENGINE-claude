#include "RdPch.h"
#include "RHI/Texture/RenderTexture.h"
#include "RHI/Device/GraphicsDevice.h"

namespace rd
{
	RenderTexture::~RenderTexture()
	{
		if (GraphicsDevice::IsAlive())
		{
			auto& device = GraphicsDevice::Get();
			device.DeferFreeDescriptor(device.GetRtvHeap(), mRtv);
		}
	}

	void RenderTexture::Create(const RenderTextureDesc& desc)
	{
		mDesc = desc;
		if (mDesc.rtvFormat == DXGI_FORMAT_UNKNOWN) { mDesc.rtvFormat = desc.format; }
		if (mDesc.srvFormat == DXGI_FORMAT_UNKNOWN) { mDesc.srvFormat = desc.format; }

		auto& device = GraphicsDevice::Get();
		auto resDesc = CD3DX12_RESOURCE_DESC::Tex2D(desc.format, desc.width, desc.height, 1, 1, 1, 0,
			D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
		D3D12_CLEAR_VALUE clear = {};
		clear.Format = mDesc.rtvFormat;
		std::memcpy(clear.Color, desc.clearColor.Data(), sizeof(float) * 4);
		CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_DEFAULT);
		ComPtr<ID3D12Resource> res;
		RD_CHECK_HR(device.GetDevice()->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &resDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clear, IID_PPV_ARGS(&res)));
		SetDebugName(res.Get(), desc.name);

		InitFromResource(res, mDesc.srvFormat, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		if (!mRtv.IsValid()) { mRtv = device.GetRtvHeap().Allocate(); }
		D3D12_RENDER_TARGET_VIEW_DESC rtv = {};
		rtv.Format = mDesc.rtvFormat;
		rtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		device.GetDevice()->CreateRenderTargetView(mResource.Get(), &rtv, mRtv.cpu);
	}

	void RenderTexture::Resize(uint32_t width, uint32_t height)
	{
		if (width == mWidth && height == mHeight && mResource) { return; }
		auto desc = mDesc;
		desc.width = std::max(1u, width);
		desc.height = std::max(1u, height);
		// RTV はすぐに書き換わるので、まだ GPU が使っているフレームとぶつからないよう新しく取り直す
		auto& device = GraphicsDevice::Get();
		device.DeferFreeDescriptor(device.GetRtvHeap(), mRtv);
		Create(desc);
	}
}
