#include "RdPch.h"
#include "RHI/GraphicsDevice.h"
#include "Core/StringUtil.h"

namespace rd
{
	namespace
	{
		constexpr uint64_t kDynamicPageSize = 8ull * 1024 * 1024;
	}

	bool GraphicsDevice::Initialize(const GraphicsDeviceDesc& desc)
	{
		RD_ASSERT(!sInstance);
		sInstance = this;
		mWidth = desc.width;
		mHeight = desc.height;
		mVSync = desc.vsync;

		CreateDevice(desc.debugLayer);

		// コマンドキュー
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		RD_CHECK_HR(mDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mQueue)));
		SetDebugName(mQueue.Get(), L"MainQueue");

		// フレームごとのコマンドアロケータ
		for (auto& frame : mFrames)
		{
			RD_CHECK_HR(mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&frame.allocator)));
		}
		RD_CHECK_HR(mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mFrames[0].allocator.Get(), nullptr, IID_PPV_ARGS(&mCmdList)));
		mCmdList->Close();

		RD_CHECK_HR(mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));
		mFenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);

		// デスクリプタヒープ
		mRtvHeap.Create(mDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 256, false);
		mDsvHeap.Create(mDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 64, false);
		mSrvHeap.Create(mDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 8192, true);

		CreateSwapChain(desc.hwnd);
		CreateBackBufferViews();

		mShaderCompiler.Initialize();
		return true;
	}

	void GraphicsDevice::Shutdown()
	{
		if (!mDevice) { return; }
		WaitIdle();
		ProcessDeferredReleases(true);
		{
			std::lock_guard lock(mUploadMutex);
			mUploads.clear();
		}
		ReleaseBackBuffers();
		for (auto& frame : mFrames)
		{
			frame.pages.clear();
			frame.allocator.Reset();
		}
		mShaderCompiler.ClearCache();
		mRtvHeap.Destroy();
		mDsvHeap.Destroy();
		mSrvHeap.Destroy();
		if (mFenceEvent) { CloseHandle(mFenceEvent); mFenceEvent = nullptr; }
		mSwapChain.Reset();
		mCmdList.Reset();
		mQueue.Reset();
		mFence.Reset();

#if RD_DEBUG
		ComPtr<ID3D12DebugDevice> debugDevice;
		if (SUCCEEDED(mDevice.As(&debugDevice)))
		{
			mDevice.Reset();
			debugDevice->ReportLiveDeviceObjects(D3D12_RLDO_SUMMARY | D3D12_RLDO_IGNORE_INTERNAL);
		}
#endif
		mDevice.Reset();
		sInstance = nullptr;
	}

	void GraphicsDevice::CreateDevice(bool debugLayer)
	{
		UINT factoryFlags = 0;
		if (debugLayer)
		{
			ComPtr<ID3D12Debug> debug;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
			{
				debug->EnableDebugLayer();
				factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
			}
		}
		RD_CHECK_HR(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&mFactory)));

		// 高性能 GPU を優先して選ぶ
		for (UINT i = 0; mFactory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&mAdapter)) != DXGI_ERROR_NOT_FOUND; ++i)
		{
			DXGI_ADAPTER_DESC1 adapterDesc = {};
			mAdapter->GetDesc1(&adapterDesc);
			if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) { continue; }
			if (SUCCEEDED(D3D12CreateDevice(mAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&mDevice))))
			{
				mAdapterName = ToUtf8(adapterDesc.Description);
				break;
			}
		}
		RD_VERIFY(mDevice, "No D3D12 capable GPU found");
		Log::Info("GPU: {}", mAdapterName);

		BOOL tearing = FALSE;
		if (SUCCEEDED(mFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &tearing, sizeof(tearing))))
		{
			mTearingSupported = tearing == TRUE;
		}

		if (debugLayer)
		{
			ComPtr<ID3D12InfoQueue> infoQueue;
			if (SUCCEEDED(mDevice.As(&infoQueue)))
			{
				// デバッガがあるときだけ止める（ないときはログに残して続行）
				BOOL brk = IsDebuggerPresent();
				infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, brk);
				infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, brk);
				// メッセージをエンジンのログへ
				ComPtr<ID3D12InfoQueue1> infoQueue1;
				if (SUCCEEDED(infoQueue.As(&infoQueue1)))
				{
					DWORD cookie = 0;
					infoQueue1->RegisterMessageCallback([](D3D12_MESSAGE_CATEGORY, D3D12_MESSAGE_SEVERITY severity, D3D12_MESSAGE_ID, LPCSTR desc, void*)
						{
							if (severity <= D3D12_MESSAGE_SEVERITY_ERROR) { Log::Error("D3D12: {}", desc); }
							else if (severity == D3D12_MESSAGE_SEVERITY_WARNING) { Log::Warning("D3D12: {}", desc); }
						}, D3D12_MESSAGE_CALLBACK_FLAG_NONE, nullptr, &cookie);
				}
				D3D12_MESSAGE_ID denyIds[] = {
					D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE,// Windows 11 の既知の問題
					D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
					D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,
				};
				D3D12_INFO_QUEUE_FILTER filter = {};
				filter.DenyList.NumIDs = _countof(denyIds);
				filter.DenyList.pIDList = denyIds;
				infoQueue->PushStorageFilter(&filter);
			}
		}
	}

	void GraphicsDevice::CreateSwapChain(HWND hwnd)
	{
		DXGI_SWAP_CHAIN_DESC1 desc = {};
		desc.Width = mWidth;
		desc.Height = mHeight;
		desc.Format = kBackBufferFormat;
		desc.SampleDesc.Count = 1;
		desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		desc.BufferCount = kBackBufferCount;
		desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		desc.Flags = mTearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

		ComPtr<IDXGISwapChain1> swapChain;
		RD_CHECK_HR(mFactory->CreateSwapChainForHwnd(mQueue.Get(), hwnd, &desc, nullptr, nullptr, &swapChain));
		RD_CHECK_HR(swapChain.As(&mSwapChain));
		// Alt+Enter による DXGI の自動フルスクリーン切り替えは使わない
		mFactory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
	}

	void GraphicsDevice::CreateBackBufferViews()
	{
		for (uint32_t i = 0; i < kBackBufferCount; ++i)
		{
			RD_CHECK_HR(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mBackBuffers[i])));
			SetDebugName(mBackBuffers[i].Get(), L"BackBuffer");
			if (!mBackBufferRtvs[i].IsValid()) { mBackBufferRtvs[i] = mRtvHeap.Allocate(); }
			mDevice->CreateRenderTargetView(mBackBuffers[i].Get(), nullptr, mBackBufferRtvs[i].cpu);
		}
		mBackBufferIndex = mSwapChain->GetCurrentBackBufferIndex();
	}

	void GraphicsDevice::ReleaseBackBuffers()
	{
		for (uint32_t i = 0; i < kBackBufferCount; ++i)
		{
			mBackBuffers[i].Reset();
			mRtvHeap.Free(mBackBufferRtvs[i]);
		}
	}

	void GraphicsDevice::BeginFrame()
	{
		FrameContext& frame = mFrames[mFrameIndex];
		// このフレームのコマンドアロケータを GPU が使い終わるまで待つ
		WaitForFence(frame.fenceValue);
		ProcessDeferredReleases(false);

		RD_CHECK_HR(frame.allocator->Reset());
		RD_CHECK_HR(mCmdList->Reset(frame.allocator.Get(), nullptr));
		frame.currentPage = 0;
		frame.pageOffset = 0;
		frame.bytesUsed = 0;

		mBackBufferIndex = mSwapChain->GetCurrentBackBufferIndex();
		ID3D12DescriptorHeap* heaps[] = { mSrvHeap.Get() };
		mCmdList->SetDescriptorHeaps(1, heaps);

		FlushUploads();
	}

	void GraphicsDevice::EndFrame()
	{
		FrameContext& frame = mFrames[mFrameIndex];
		RD_CHECK_HR(mCmdList->Close());
		ID3D12CommandList* lists[] = { mCmdList.Get() };
		mQueue->ExecuteCommandLists(1, lists);

		UINT flags = (!mVSync && mTearingSupported) ? DXGI_PRESENT_ALLOW_TEARING : 0;
		HRESULT hr = mSwapChain->Present(mVSync ? 1 : 0, flags);
		if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
		{
			RD_VERIFY(false, "GPU device removed (0x{:08X})", uint32_t(mDevice->GetDeviceRemovedReason()));
		}

		frame.fenceValue = mNextFenceValue++;
		RD_CHECK_HR(mQueue->Signal(mFence.Get(), frame.fenceValue));
		mLastDynamicBytes = frame.bytesUsed;

		mFrameIndex = (mFrameIndex + 1) % kFrameCount;
		++mFrameNumber;
	}

	void GraphicsDevice::Resize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0 || (width == mWidth && height == mHeight)) { return; }
		WaitIdle();
		ReleaseBackBuffers();
		mWidth = width;
		mHeight = height;
		RD_CHECK_HR(mSwapChain->ResizeBuffers(kBackBufferCount, width, height, kBackBufferFormat,
			mTearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0));
		CreateBackBufferViews();
	}

	void GraphicsDevice::WaitIdle()
	{
		if (!mQueue) { return; }
		uint64_t value = mNextFenceValue++;
		mQueue->Signal(mFence.Get(), value);
		WaitForFence(value);
	}

	void GraphicsDevice::WaitForFence(uint64_t value)
	{
		if (value == 0 || mFence->GetCompletedValue() >= value) { return; }
		mFence->SetEventOnCompletion(value, mFenceEvent);
		WaitForSingleObject(mFenceEvent, INFINITE);
	}

	DynamicAllocation GraphicsDevice::AllocateDynamic(uint64_t size, uint64_t alignment)
	{
		FrameContext& frame = mFrames[mFrameIndex];
		auto alignUp = [](uint64_t v, uint64_t a) { return (v + a - 1) & ~(a - 1); };

		while (true)
		{
			if (frame.currentPage >= frame.pages.size())
			{
				// 新しいページを作成（大きな要求には専用サイズ）
				DynamicPage page;
				page.size = std::max(kDynamicPageSize, alignUp(size, 65536));
				page.resource = CreateStagingBuffer(page.size);
				SetDebugName(page.resource.Get(), L"DynamicUploadPage");
				D3D12_RANGE readRange = { 0, 0 };
				page.resource->Map(0, &readRange, reinterpret_cast<void**>(&page.cpu));
				page.gpu = page.resource->GetGPUVirtualAddress();
				frame.pages.push_back(std::move(page));
			}
			DynamicPage& page = frame.pages[frame.currentPage];
			uint64_t offset = alignUp(frame.pageOffset, alignment);
			if (offset + size <= page.size)
			{
				frame.pageOffset = offset + size;
				frame.bytesUsed += size;
				return { page.cpu + offset, page.gpu + offset, page.resource.Get(), offset };
			}
			++frame.currentPage;
			frame.pageOffset = 0;
		}
	}

	void GraphicsDevice::DeferRelease(ComPtr<ID3D12Resource> resource)
	{
		if (!resource) { return; }
		std::lock_guard lock(mDeferredMutex);
		mDeferred.push_back({ mNextFenceValue, std::move(resource), nullptr, {} });
	}

	void GraphicsDevice::DeferFreeDescriptor(DescriptorHeap& heap, DescriptorHandle& handle)
	{
		if (!handle.IsValid()) { return; }
		std::lock_guard lock(mDeferredMutex);
		mDeferred.push_back({ mNextFenceValue, nullptr, &heap, handle });
		handle = {};
	}

	void GraphicsDevice::ProcessDeferredReleases(bool all)
	{
		uint64_t completed = mFence->GetCompletedValue();
		std::lock_guard lock(mDeferredMutex);
		auto it = std::remove_if(mDeferred.begin(), mDeferred.end(), [&](DeferredItem& item)
			{
				if (!all && item.fenceValue > completed) { return false; }
				if (item.heap) { item.heap->Free(item.handle); }
				return true;
			});
		mDeferred.erase(it, mDeferred.end());
	}

	ComPtr<ID3D12Resource> GraphicsDevice::CreateStagingBuffer(uint64_t size, const void* data)
	{
		ComPtr<ID3D12Resource> buffer;
		CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_UPLOAD);
		auto desc = CD3DX12_RESOURCE_DESC::Buffer(size);
		RD_CHECK_HR(mDevice->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buffer)));
		if (data)
		{
			void* mapped = nullptr;
			D3D12_RANGE readRange = { 0, 0 };
			buffer->Map(0, &readRange, &mapped);
			std::memcpy(mapped, data, size);
			buffer->Unmap(0, nullptr);
		}
		return buffer;
	}

	ComPtr<ID3D12Resource> GraphicsDevice::CreateStaticBuffer(const void* data, uint64_t size,
		D3D12_RESOURCE_STATES finalState, std::function<void()> onComplete)
	{
		ComPtr<ID3D12Resource> buffer;
		CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_DEFAULT);
		auto desc = CD3DX12_RESOURCE_DESC::Buffer(size);
		RD_CHECK_HR(mDevice->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
			D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&buffer)));

		UploadRequest request;
		request.dst = buffer;
		request.staging = CreateStagingBuffer(size, data);
		request.bufferSize = size;
		request.finalState = finalState;
		request.onComplete = std::move(onComplete);
		EnqueueUpload(std::move(request));
		return buffer;
	}

	void GraphicsDevice::EnqueueUpload(UploadRequest request)
	{
		std::lock_guard lock(mUploadMutex);
		mUploads.push_back(std::move(request));
	}

	void GraphicsDevice::FlushUploads()
	{
		std::vector<UploadRequest> uploads;
		{
			std::lock_guard lock(mUploadMutex);
			uploads.swap(mUploads);
		}
		if (uploads.empty()) { return; }

		std::vector<D3D12_RESOURCE_BARRIER> barriers;
		for (auto& up : uploads)
		{
			if (up.isTexture)
			{
				for (UINT i = 0; i < UINT(up.footprints.size()); ++i)
				{
					CD3DX12_TEXTURE_COPY_LOCATION dst(up.dst.Get(), i);
					CD3DX12_TEXTURE_COPY_LOCATION src(up.staging.Get(), up.footprints[i]);
					mCmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
				}
				barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(up.dst.Get(), D3D12_RESOURCE_STATE_COPY_DEST, up.finalState));
			}
			else
			{
				TransitionBarrier(mCmdList.Get(), up.dst.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
				mCmdList->CopyBufferRegion(up.dst.Get(), 0, up.staging.Get(), 0, up.bufferSize);
				barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(up.dst.Get(), D3D12_RESOURCE_STATE_COPY_DEST, up.finalState));
			}
		}
		mCmdList->ResourceBarrier(UINT(barriers.size()), barriers.data());

		// コピーはこのフレームのコマンドリストの先頭で実行されるので、以降の描画から使ってよい
		for (auto& up : uploads)
		{
			DeferRelease(std::move(up.staging));
			if (up.onComplete) { up.onComplete(); }
		}
	}
}
