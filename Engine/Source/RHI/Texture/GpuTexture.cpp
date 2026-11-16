#include "RdPch.h"
#include "RHI/Texture/GpuTexture.h"
#include "RHI/Device/GraphicsDevice.h"

namespace rd
{
	GpuTexture::~GpuTexture()
	{
		Release();
	}

	void GpuTexture::Release()
	{
		if (!GraphicsDevice::IsAlive()) { mResource.Reset(); mSrv = {}; return; }
		auto& device = GraphicsDevice::Get();
		device.DeferRelease(std::move(mResource));
		device.DeferFreeDescriptor(device.GetSrvHeap(), mSrv);
	}

	void GpuTexture::InitFromResource(ComPtr<ID3D12Resource> resource, DXGI_FORMAT srvFormat, D3D12_RESOURCE_STATES state)
	{
		Release();
		auto& device = GraphicsDevice::Get();
		mResource = std::move(resource);
		mState = state;
		auto desc = mResource->GetDesc();
		mWidth = uint32_t(desc.Width);
		mHeight = desc.Height;

		mSrv = device.GetSrvHeap().Allocate();
		D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
		srv.Format = srvFormat == DXGI_FORMAT_UNKNOWN ? desc.Format : srvFormat;
		srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srv.Texture2D.MipLevels = desc.MipLevels;
		device.GetDevice()->CreateShaderResourceView(mResource.Get(), &srv, mSrv.cpu);
	}

	void GpuTexture::CreateSolidColor(uint32_t rgba)
	{
		auto& device = GraphicsDevice::Get();
		auto desc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 1, 1);
		CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_DEFAULT);
		ComPtr<ID3D12Resource> tex;
		RD_CHECK_HR(device.GetDevice()->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
			D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&tex)));

		UploadRequest req;
		req.isTexture = true;
		req.footprints.resize(1);
		UINT64 total = 0;
		device.GetDevice()->GetCopyableFootprints(&desc, 0, 1, 0, req.footprints.data(), nullptr, nullptr, &total);
		std::vector<uint8_t> data(size_t(total), 0);
		std::memcpy(data.data() + req.footprints[0].Offset, &rgba, 4);
		req.staging = device.CreateStagingBuffer(total, data.data());
		req.dst = tex;
		req.finalState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		device.EnqueueUpload(std::move(req));

		InitFromResource(tex, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	}
}
