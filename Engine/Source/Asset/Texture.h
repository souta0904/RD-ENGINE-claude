#pragma once
#include "Asset/Asset.h"
#include "RHI/GpuTexture.h"
#include <vector>

namespace rd
{
	// 画像ファイルから読み込むテクスチャ（png/jpg/bmp/tga/dds/hdr）
	//  キーの末尾が "#linear" ならリニア（法線マップなど）、それ以外は sRGB として扱う
	class Texture final : public Asset
	{
	public:
		const char* GetTypeName() const override { return "Texture"; }

		const GpuTexture& GetGpu() const { return mGpu; }
		D3D12_GPU_DESCRIPTOR_HANDLE GetSrv() const { return mGpu.GetGpuSrv(); }
		uint32_t GetWidth() const { return mGpu.GetWidth(); }
		uint32_t GetHeight() const { return mGpu.GetHeight(); }

		// メモリ上の画像（モデル埋め込みなど）から読み込む。ワーカースレッドから呼んでよい
		void LoadFromMemory(const void* data, size_t size, bool srgb);
		// 生の RGBA8 ピクセルから作成
		void LoadFromPixels(const void* rgba, uint32_t width, uint32_t height, bool srgb);

	protected:
		void Load() override;

	private:
		friend class AssetManager;
		bool UploadScratch(const void* scratchImage, bool srgb);

		GpuTexture mGpu;
	};
}
