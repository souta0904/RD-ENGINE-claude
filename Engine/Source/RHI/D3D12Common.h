#pragma once
#include <d3d12.h>
#include <d3dx12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

namespace rd
{
	using Microsoft::WRL::ComPtr;

	// 同時に処理するフレーム数（CPU と GPU の並行動作）
	inline constexpr uint32_t kFrameCount = 2;

	// HRESULT チェック（失敗は致命的エラー扱い）
#define RD_CHECK_HR(expr)                                                         \
	do                                                                            \
	{                                                                             \
		HRESULT hr__ = (expr);                                                    \
		RD_VERIFY(SUCCEEDED(hr__), "{} failed (0x{:08X})", #expr, uint32_t(hr__)); \
	} while (false)

	inline void TransitionBarrier(ID3D12GraphicsCommandList* cmd, ID3D12Resource* resource,
		D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
	{
		if (before == after) { return; }
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource, before, after);
		cmd->ResourceBarrier(1, &barrier);
	}

	inline void SetDebugName(ID3D12Object* object, const wchar_t* name)
	{
#if RD_DEBUG
		if (object && name) { object->SetName(name); }
#else
		(void)object; (void)name;
#endif
	}
}
