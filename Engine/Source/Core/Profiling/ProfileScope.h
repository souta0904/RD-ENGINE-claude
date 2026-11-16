#pragma once
#include "Core/Profiling/Profiler.h"

// RD_PROFILER=0 にすると計測のコードを丸ごと外す（既定はすべての構成で有効。1 区間あたり数十ナノ秒）
#ifndef RD_PROFILER
#define RD_PROFILER 1
#endif

namespace rd
{
	// 区間を計測する（スコープを抜けると終わる）
	class ProfileScope
	{
	public:
		explicit ProfileScope(const char* name) { Profiler::BeginScope(name); }
		~ProfileScope() { Profiler::EndScope(); }
		ProfileScope(const ProfileScope&) = delete;
		ProfileScope& operator=(const ProfileScope&) = delete;
	};
}

#define RD_PROFILE_CONCAT_(a, b) a##b
#define RD_PROFILE_CONCAT(a, b) RD_PROFILE_CONCAT_(a, b)
#if RD_PROFILER
#define RD_PROFILE_SCOPE(name) ::rd::ProfileScope RD_PROFILE_CONCAT(rdProfileScope_, __LINE__)(name)
#else
#define RD_PROFILE_SCOPE(name) ((void)0)
#endif
#define RD_PROFILE_FUNCTION() RD_PROFILE_SCOPE(__FUNCTION__)
