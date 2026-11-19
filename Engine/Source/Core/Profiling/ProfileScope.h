#pragma once
#include "Core/Profiling/Profiler.h"

// RD_PROFILER=0 にすると計測のコードを丸ごと外す（既定はすべての構成で有効。1 区間あたり数十ナノ秒）
#ifndef RD_PROFILER
#define RD_PROFILER 1
#endif

namespace rd
{
	// 区間を計測する（スコープを抜けると終わる）
	//  コンストラクタで BeginScope、デストラクタで EndScope を呼ぶ（ScopedTimer と同じ RAII の形）。
	//  途中で return しても必ず終わりが記録されるので、Begin と End の数がずれない
	//  コピーできると End が 2 回呼ばれてしまうので、コピーを禁止する（= delete）
	class ProfileScope
	{
	public:
		explicit ProfileScope(const char* name) { Profiler::BeginScope(name); }
		~ProfileScope() { Profiler::EndScope(); }
		ProfileScope(const ProfileScope&) = delete;
		ProfileScope& operator=(const ProfileScope&) = delete;
	};
}

// 使い方: 関数やブロックの先頭に書く
//   void Renderer::Draw()
//   {
//       RD_PROFILE_SCOPE("Renderer::Draw");
//       ...
//   }
//
// 変数の名前を rdProfileScope_120 のように行番号付きにして、同じ関数に 2 つ書いても名前がぶつからないようにする。
// ## は 2 つの字句をつなげる演算子。ただし ## の隣にある引数は展開されないので、__LINE__ をそのまま渡すと
// "rdProfileScope___LINE__" になってしまう。CONCAT を 2 段にして、先に __LINE__ を 120 へ展開させてからつなげる
#define RD_PROFILE_CONCAT_(a, b) a##b
#define RD_PROFILE_CONCAT(a, b) RD_PROFILE_CONCAT_(a, b)
#if RD_PROFILER
#define RD_PROFILE_SCOPE(name) ::rd::ProfileScope RD_PROFILE_CONCAT(rdProfileScope_, __LINE__)(name)
#else
#define RD_PROFILE_SCOPE(name) ((void)0)
#endif
// 関数の名前をそのまま区間の名前にする（__FUNCTION__ はコンパイラが入れる今の関数の名前）
#define RD_PROFILE_FUNCTION() RD_PROFILE_SCOPE(__FUNCTION__)
