#include "RdPch.h"
#include "Render/Features/ScreenshotFeature.h"
#include "Core/Util/StringUtil.h"
#include "Render/Renderer/FrameContext.h"
#include "RHI/Device/GraphicsDevice.h"
#include <DirectXTex.h>

namespace rd
{
	void ScreenshotFeature::AddPasses(rg::RenderGraph& graph, FrameContext& frame)
	{
		if (mRequestedPath.empty() || !frame.isMain) { return; }
		const rg::TextureHandle output = frame.textures.output;
		graph.AddPass("Capture", [&](rg::PassBuilder& b) { b.CopyFrom(output); b.SideEffect(); },
			[this, output](rg::PassContext& c) { CopyToReadback(c.cmd, c.Color(output)); });
	}

	void ScreenshotFeature::CopyToReadback(ID3D12GraphicsCommandList* cmd, RenderTexture& output)
	{
		auto& device = GraphicsDevice::Get();
		auto desc = output.GetResource()->GetDesc();
		UINT64 total = 0;
		device.GetDevice()->GetCopyableFootprints(&desc, 0, 1, 0, &mFootprint, nullptr, nullptr, &total);
		CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_READBACK);
		UINT64 size = std::max<UINT64>(total, UINT64(mFootprint.Footprint.RowPitch) * mFootprint.Footprint.Height);
		auto bufDesc = CD3DX12_RESOURCE_DESC::Buffer(size);
		mBuffer.Reset();
		RD_CHECK_HR(device.GetDevice()->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &bufDesc,
			D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&mBuffer)));
		CD3DX12_TEXTURE_COPY_LOCATION dst(mBuffer.Get(), mFootprint);
		CD3DX12_TEXTURE_COPY_LOCATION src(output.GetResource(), 0);
		cmd->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
		mPendingPath = std::move(mRequestedPath);
		mRequestedPath.clear();
	}

	void ScreenshotFeature::Finish()
	{
		if (mPendingPath.empty() || !mBuffer) { return; }
		GraphicsDevice::Get().WaitIdle();
		uint8_t* data = nullptr;
		D3D12_RANGE range = { 0, SIZE_T(mFootprint.Footprint.RowPitch) * mFootprint.Footprint.Height };
		if (SUCCEEDED(mBuffer->Map(0, &range, reinterpret_cast<void**>(&data))))
		{
			DirectX::Image image = {};
			image.width = mFootprint.Footprint.Width;
			image.height = mFootprint.Footprint.Height;
			image.format = DXGI_FORMAT_R8G8B8A8_UNORM;
			image.rowPitch = mFootprint.Footprint.RowPitch;
			image.slicePitch = image.rowPitch * image.height;
			image.pixels = data;
			std::filesystem::path path(ToWide(mPendingPath));
			if (path.has_parent_path()) { std::filesystem::create_directories(path.parent_path()); }
			HRESULT hr = DirectX::SaveToWICFile(image, DirectX::WIC_FLAGS_NONE,
				DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG), path.c_str());
			mBuffer->Unmap(0, nullptr);
			if (SUCCEEDED(hr)) { Logger::Info(LogCat::Render, "Saved screenshot: {}", mPendingPath); }
			else { Logger::Error(LogCat::Render, "Failed to save screenshot: {}", mPendingPath); }
		}
		mPendingPath.clear();
		mBuffer.Reset();
	}
}
