#pragma once
#include "RHI/Descriptor/DescriptorHeap.h"
#include "RHI/Device/DynamicAllocation.h"
#include "RHI/Device/GraphicsDeviceDesc.h"
#include "RHI/Device/UploadRequest.h"
#include "RHI/Shader/ShaderCompiler.h"
#include <mutex>
#include <string>

namespace rd
{
	// D3D12 のデバイスとフレームの管理
	//  - kFrameCount フレーム分のコマンドアロケータを持ち、CPU と GPU を同時に動かす
	//  - GPU が使っているかもしれないリソースは DeferRelease で、使い終わってから解放する
	//  - どのスレッドからでも EnqueueUpload でバッファ / テクスチャのアップロードを予約できる
	class RD_API GraphicsDevice
	{
	public:
		static constexpr DXGI_FORMAT kBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		static constexpr uint32_t kBackBufferCount = 3;

		static GraphicsDevice& Get() { return *sInstance; }
		static bool IsAlive() { return sInstance != nullptr; }

		bool Initialize(const GraphicsDeviceDesc& desc);
		void Shutdown();

		void BeginFrame();
		void EndFrame();
		void Resize(uint32_t width, uint32_t height);
		void WaitIdle();

		DynamicAllocation AllocateDynamic(uint64_t size, uint64_t alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		template <class T>
		D3D12_GPU_VIRTUAL_ADDRESS UploadConstants(const T& data)
		{
			auto alloc = AllocateDynamic(sizeof(T));
			std::memcpy(alloc.cpu, &data, sizeof(T));
			return alloc.gpu;
		}
		D3D12_GPU_VIRTUAL_ADDRESS UploadData(const void* data, uint64_t size, uint64_t alignment = 16)
		{
			auto alloc = AllocateDynamic(size, alignment);
			std::memcpy(alloc.cpu, data, size);
			return alloc.gpu;
		}

		// GPU の区間の計測（結果はプロファイラへ。PIX / RenderDoc のイベント名にもなる）
		//  name は文字列リテラル。RD_GPU_SCOPE("Shadow") を使うと閉じ忘れない
		void BeginGpuScope(const char* name);
		void EndGpuScope();

		void DeferRelease(ComPtr<ID3D12Resource> resource);
		void DeferFreeDescriptor(DescriptorHeap& heap, DescriptorHandle& handle);

		void EnqueueUpload(UploadRequest request);
		ComPtr<ID3D12Resource> CreateStagingBuffer(uint64_t size, const void* data = nullptr);
		// 初期データ付きの DEFAULT ヒープのバッファ
		ComPtr<ID3D12Resource> CreateStaticBuffer(const void* data, uint64_t size,
			D3D12_RESOURCE_STATES finalState, std::function<void()> onComplete = nullptr);

		ID3D12Device* GetDevice() const { return mDevice.Get(); }
		ID3D12GraphicsCommandList* GetCommandList() const { return mCmdList.Get(); }
		ID3D12CommandQueue* GetQueue() const { return mQueue.Get(); }
		DescriptorHeap& GetRtvHeap() { return mRtvHeap; }
		DescriptorHeap& GetDsvHeap() { return mDsvHeap; }
		DescriptorHeap& GetSrvHeap() { return mSrvHeap; }
		ShaderCompiler& GetShaderCompiler() { return mShaderCompiler; }
		ID3D12Resource* GetBackBuffer() const { return mBackBuffers[mBackBufferIndex].Get(); }
		D3D12_CPU_DESCRIPTOR_HANDLE GetBackBufferRtv() const { return mBackBufferRtvs[mBackBufferIndex].cpu; }
		uint32_t GetWidth() const { return mWidth; }
		uint32_t GetHeight() const { return mHeight; }
		uint64_t GetFrameNumber() const { return mFrameNumber; }
		uint32_t GetFrameIndex() const { return mFrameIndex; }
		void SetVSync(bool vsync) { mVSync = vsync; }
		bool GetVSync() const { return mVSync; }
		uint64_t GetDynamicBytesUsed() const { return mLastDynamicBytes; }
		const std::string& GetAdapterName() const { return mAdapterName; }

	private:
		void CreateDevice(bool debugLayer);
		void SetupDebugMessages();
		void CreateSwapChain(HWND hwnd);
		void CreateBackBufferViews();
		void ReleaseBackBuffers();
		void CreateTimestampQueries();
		void WaitForFence(uint64_t value);
		void ProcessDeferredReleases(bool all);
		void FlushUploads();

		struct DynamicPage
		{
			ComPtr<ID3D12Resource> resource;
			uint8_t* cpu = nullptr;
			D3D12_GPU_VIRTUAL_ADDRESS gpu = 0;
			uint64_t size = 0;
		};
		// begin / end はタイムスタンプの番号
		struct GpuScopeRecord
		{
			const char* name;
			uint32_t begin;
			uint32_t end;
			uint16_t depth;
		};
		struct FrameContext
		{
			ComPtr<ID3D12CommandAllocator> allocator;
			uint64_t fenceValue = 0;
			std::vector<DynamicPage> pages;
			uint32_t currentPage = 0;
			uint64_t pageOffset = 0;
			uint64_t bytesUsed = 0;
			std::vector<GpuScopeRecord> gpuScopes;
			uint32_t queryCount = 0;
			uint64_t profilerFrame = 0;
		};
		struct DeferredItem
		{
			uint64_t fenceValue = 0;
			ComPtr<ID3D12Resource> resource;
			DescriptorHeap* heap = nullptr;
			DescriptorHandle handle;
		};
		// 1 フレームあたりのタイムスタンプの数（区間 64 個分）
		static constexpr uint32_t kMaxGpuQueries = 128;
		void CollectGpuTimings(FrameContext& frame);

		static inline GraphicsDevice* sInstance = nullptr;

		ComPtr<IDXGIFactory6> mFactory;
		ComPtr<IDXGIAdapter1> mAdapter;
		ComPtr<ID3D12Device> mDevice;
		ComPtr<ID3D12CommandQueue> mQueue;
		ComPtr<ID3D12GraphicsCommandList> mCmdList;
		ComPtr<IDXGISwapChain4> mSwapChain;
		ComPtr<ID3D12Fence> mFence;
		HANDLE mFenceEvent = nullptr;
		uint64_t mNextFenceValue = 1;

		FrameContext mFrames[kFrameCount];
		uint32_t mFrameIndex = 0;
		uint64_t mFrameNumber = 0;

		ComPtr<ID3D12Resource> mBackBuffers[kBackBufferCount];
		DescriptorHandle mBackBufferRtvs[kBackBufferCount];
		uint32_t mBackBufferIndex = 0;

		DescriptorHeap mRtvHeap;
		DescriptorHeap mDsvHeap;
		DescriptorHeap mSrvHeap;

		std::mutex mDeferredMutex;
		std::vector<DeferredItem> mDeferred;

		std::mutex mUploadMutex;
		std::vector<UploadRequest> mUploads;

		ShaderCompiler mShaderCompiler;

		ComPtr<ID3D12QueryHeap> mTimestampHeap;
		ComPtr<ID3D12Resource> mTimestampReadback;
		uint64_t mTimestampFrequency = 0;
		std::vector<uint32_t> mGpuScopeStack;

		uint32_t mWidth = 0;
		uint32_t mHeight = 0;
		bool mVSync = true;
		bool mTearingSupported = false;
		uint64_t mLastDynamicBytes = 0;
		std::string mAdapterName;
	};
}
