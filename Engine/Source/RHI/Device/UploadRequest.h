#pragma once
#include "RHI/D3D12Common.h"
#include <functional>
#include <vector>

namespace rd
{
	// GPU へのアップロードの予約
	//  staging の中身を次のフレームの最初に dst へコピーし、finalState にしてから onComplete をメインスレッドで呼ぶ
	struct UploadRequest
	{
		ComPtr<ID3D12Resource> dst;
		ComPtr<ID3D12Resource> staging;
		bool isTexture = false;
		uint64_t bufferSize = 0;
		std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints;
		D3D12_RESOURCE_STATES finalState = D3D12_RESOURCE_STATE_COMMON;
		std::function<void()> onComplete;
	};
}
