#pragma once
#include "Core/Profiling/ProfileScope.h"
#include "RHI/Device/GraphicsDevice.h"

namespace rd
{
	// GPU の区間を計測する（スコープを抜けると終わる）
	class GpuScope
	{
	public:
		explicit GpuScope(const char* name) { GraphicsDevice::Get().BeginGpuScope(name); }
		~GpuScope() { GraphicsDevice::Get().EndGpuScope(); }
		GpuScope(const GpuScope&) = delete;
		GpuScope& operator=(const GpuScope&) = delete;
	};
}

// CPU と GPU の両方を同じ名前で計測する
#define RD_GPU_SCOPE(name)                                                       \
	RD_PROFILE_SCOPE(name);                                                      \
	::rd::GpuScope RD_PROFILE_CONCAT(rdGpuScope_, __LINE__)(name)
