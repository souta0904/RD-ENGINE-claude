#pragma once
#include "Math/Color.h"
#include "RHI/D3D12Common.h"
#include "RHI/DescriptorHeap.h"

namespace rd
{
	// SRV を持つ GPU テクスチャ
	class GpuTexture
	{
	public:
		GpuTexture() = default;
		~GpuTexture();
		GpuTexture(const GpuTexture&) = delete;
		GpuTexture& operator=(const GpuTexture&) = delete;

		// 既存リソースから SRV を作成して保持
		void InitFromResource(ComPtr<ID3D12Resource> resource, DXGI_FORMAT srvFormat, D3D12_RESOURCE_STATES state);
		// 単色 1x1 テクスチャ（同期アップロード予約）
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

	struct RenderTextureDesc
	{
		uint32_t width = 0;
		uint32_t height = 0;
		DXGI_FORMAT format = DXGI_FORMAT_R16G16B16A16_FLOAT;// リソースの形式（TYPELESS も可）
		DXGI_FORMAT rtvFormat = DXGI_FORMAT_UNKNOWN;// UNKNOWN なら format と同じ
		DXGI_FORMAT srvFormat = DXGI_FORMAT_UNKNOWN;
		Color clearColor = Color::kClear;
		const wchar_t* name = L"RenderTexture";
	};

	// レンダーターゲット（カラー）
	class RenderTexture : public GpuTexture
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

	// 深度バッファ（SRV として読める）
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

	// ビューポートとシザーを設定
	void SetViewport(ID3D12GraphicsCommandList* cmd, uint32_t width, uint32_t height);
}
