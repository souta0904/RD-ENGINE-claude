#include "RdPch.h"
#include "RHI/Texture/DepthTexture.h"
#include "RHI/Device/GraphicsDevice.h"

namespace rd
{
	DepthTexture::~DepthTexture()
	{
		if (GraphicsDevice::IsAlive())
		{
			auto& device = GraphicsDevice::Get();
			device.DeferFreeDescriptor(device.GetDsvHeap(), mDsv);
		}
	}

	void DepthTexture::Create(uint32_t width, uint32_t height, const wchar_t* name)
	{
		mName = name;
		auto& device = GraphicsDevice::Get();
		auto resDesc = CD3DX12_RESOURCE_DESC::Tex2D(kResourceFormat, std::max(1u, width), std::max(1u, height), 1, 1, 1, 0,
			D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
		D3D12_CLEAR_VALUE clear = {};
		clear.Format = kDsvFormat;
		clear.DepthStencil.Depth = 1.0f;
		CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_DEFAULT);
		ComPtr<ID3D12Resource> res;
		RD_CHECK_HR(device.GetDevice()->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &resDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear, IID_PPV_ARGS(&res)));
		SetDebugName(res.Get(), name);

		InitFromResource(res, kSrvFormat, D3D12_RESOURCE_STATE_DEPTH_WRITE);

		if (!mDsv.IsValid()) { mDsv = device.GetDsvHeap().Allocate(); }
		D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
		dsv.Format = kDsvFormat;
		dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		device.GetDevice()->CreateDepthStencilView(mResource.Get(), &dsv, mDsv.cpu);
	}

	void DepthTexture::Resize(uint32_t width, uint32_t height)
	{
		if (width == mWidth && height == mHeight && mResource) { return; }
		auto& device = GraphicsDevice::Get();
		device.DeferFreeDescriptor(device.GetDsvHeap(), mDsv);
		Create(width, height, mName);
	}
}
