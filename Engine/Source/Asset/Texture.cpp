#include "RdPch.h"
#include "Asset/Texture.h"
#include "Core/StringUtil.h"
#include "RHI/GraphicsDevice.h"
#include <DirectXTex.h>

namespace rd
{
	void Texture::Load()
	{
		std::string path = mPath;
		bool srgb = true;
		if (EndsWith(path, "#linear"))
		{
			srgb = false;
			path.resize(path.size() - std::strlen("#linear"));
		}

		std::wstring wpath = ToWide(path);
		std::string ext = GetExtension(path);
		DirectX::ScratchImage image;
		HRESULT hr;
		if (ext == ".dds") { hr = DirectX::LoadFromDDSFile(wpath.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image); }
		else if (ext == ".tga") { hr = DirectX::LoadFromTGAFile(wpath.c_str(), nullptr, image); }
		else if (ext == ".hdr") { hr = DirectX::LoadFromHDRFile(wpath.c_str(), nullptr, image); }
		else
		{
			hr = DirectX::LoadFromWICFile(wpath.c_str(),
				srgb ? DirectX::WIC_FLAGS_FORCE_SRGB : DirectX::WIC_FLAGS_IGNORE_SRGB, nullptr, image);
		}
		if (FAILED(hr))
		{
			Log::Error(LogCat::Asset, "Failed to load texture: {}", path);
			SetState(AssetState::Failed);
			return;
		}
		if (!UploadScratch(&image, srgb)) { SetState(AssetState::Failed); }
	}

	void Texture::LoadFromMemory(const void* data, size_t size, bool srgb)
	{
		DirectX::ScratchImage image;
		HRESULT hr = DirectX::LoadFromWICMemory(data, size,
			srgb ? DirectX::WIC_FLAGS_FORCE_SRGB : DirectX::WIC_FLAGS_IGNORE_SRGB, nullptr, image);
		if (FAILED(hr))
		{
			hr = DirectX::LoadFromTGAMemory(data, size, nullptr, image);
		}
		if (FAILED(hr) || !UploadScratch(&image, srgb))
		{
			Log::Error(LogCat::Asset, "Failed to load texture from memory: {}", mPath);
			SetState(AssetState::Failed);
		}
	}

	void Texture::LoadFromPixels(const void* rgba, uint32_t width, uint32_t height, bool srgb)
	{
		DirectX::ScratchImage image;
		image.Initialize2D(srgb ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, 1);
		std::memcpy(image.GetPixels(), rgba, size_t(width) * height * 4);
		if (!UploadScratch(&image, srgb)) { SetState(AssetState::Failed); }
	}

	bool Texture::UploadScratch(const void* scratchPtr, bool srgb)
	{
		auto& source = *static_cast<const DirectX::ScratchImage*>(scratchPtr);
		const DirectX::ScratchImage* image = &source;

		// ミップマップを生成（圧縮形式でなく 1 段しかない場合）
		DirectX::ScratchImage mipChain;
		const auto& meta = source.GetMetadata();
		if (meta.mipLevels == 1 && !DirectX::IsCompressed(meta.format) && (meta.width > 1 || meta.height > 1))
		{
			if (SUCCEEDED(DirectX::GenerateMipMaps(source.GetImages(), source.GetImageCount(), meta,
				srgb ? DirectX::TEX_FILTER_SRGB : DirectX::TEX_FILTER_DEFAULT, 0, mipChain)))
			{
				image = &mipChain;
			}
		}

		auto& device = GraphicsDevice::Get();
		ComPtr<ID3D12Resource> resource;
		HRESULT hr = DirectX::CreateTextureEx(device.GetDevice(), image->GetMetadata(), D3D12_RESOURCE_FLAG_NONE,
			srgb ? DirectX::CREATETEX_FORCE_SRGB : DirectX::CREATETEX_DEFAULT, resource.GetAddressOf());
		if (FAILED(hr)) { return false; }
		SetDebugName(resource.Get(), ToWide(mPath).c_str());

		std::vector<D3D12_SUBRESOURCE_DATA> subresources;
		hr = DirectX::PrepareUpload(device.GetDevice(), image->GetImages(), image->GetImageCount(), image->GetMetadata(), subresources);
		if (FAILED(hr)) { return false; }

		// ステージングバッファへ行ごとにコピー
		auto desc = resource->GetDesc();
		UINT count = UINT(subresources.size());
		GraphicsDevice::UploadRequest req;
		req.isTexture = true;
		req.footprints.resize(count);
		std::vector<UINT> rows(count);
		std::vector<UINT64> rowSizes(count);
		UINT64 total = 0;
		device.GetDevice()->GetCopyableFootprints(&desc, 0, count, 0, req.footprints.data(), rows.data(), rowSizes.data(), &total);

		req.staging = device.CreateStagingBuffer(total);
		uint8_t* mapped = nullptr;
		D3D12_RANGE readRange = { 0, 0 };
		req.staging->Map(0, &readRange, reinterpret_cast<void**>(&mapped));
		for (UINT i = 0; i < count; ++i)
		{
			const auto& fp = req.footprints[i];
			const auto& src = subresources[i];
			for (UINT z = 0; z < fp.Footprint.Depth; ++z)
			{
				for (UINT y = 0; y < rows[i]; ++y)
				{
					std::memcpy(mapped + fp.Offset + UINT64(fp.Footprint.RowPitch) * (rows[i] * z + y),
						static_cast<const uint8_t*>(src.pData) + src.SlicePitch * z + src.RowPitch * y,
						size_t(rowSizes[i]));
				}
			}
		}
		req.staging->Unmap(0, nullptr);

		req.dst = resource;
		req.finalState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		auto finalState = req.finalState;
		DXGI_FORMAT srvFormat = desc.Format;
		// SRV はワーカーで作っておき、GPU 転送が終わったら Ready にする
		mGpu.InitFromResource(resource, srvFormat, finalState);
		// 転送完了前にアセットが破棄されないよう自分自身を保持しておく
		auto self = std::static_pointer_cast<Texture>(shared_from_this());
		req.onComplete = [self] { self->SetState(AssetState::Ready); };
		device.EnqueueUpload(std::move(req));
		return true;
	}
}
