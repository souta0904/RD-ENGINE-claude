#include "RdPch.h"
#include "RHI/GpuTexture.h"
#include "RHI/GraphicsDevice.h"

namespace rd
{
	// ==================================================
	// GpuTexture
	// ==================================================
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

		GraphicsDevice::UploadRequest req;
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

	// ==================================================
	// RenderTexture
	// ==================================================
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
		// RTV は即座に書き換えるので、使用中フレームとの競合を避けるため新しく確保する
		auto& device = GraphicsDevice::Get();
		device.DeferFreeDescriptor(device.GetRtvHeap(), mRtv);
		Create(desc);
	}

	// ==================================================
	// DepthTexture
	// ==================================================
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

	void SetViewport(ID3D12GraphicsCommandList* cmd, uint32_t width, uint32_t height)
	{
		D3D12_VIEWPORT vp = { 0.0f, 0.0f, float(width), float(height), 0.0f, 1.0f };
		D3D12_RECT rect = { 0, 0, LONG(width), LONG(height) };
		cmd->RSSetViewports(1, &vp);
		cmd->RSSetScissorRects(1, &rect);
	}
}
