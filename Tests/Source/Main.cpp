#include "TestFramework.h"
#include "Core/Log.h"
#include <Windows.h>
#include <cstdio>
#include <string>

// テストの実行ファイル
//  Tests.exe            すべて実行
//  Tests.exe Scene      名前（Suite.Name）に "Scene" を含むものだけ実行
//  失敗した数を終了コードで返す（0 なら成功）
namespace crashtest
{
	int Run(const std::string& kind, const std::string& dir);
}

int main(int argc, char** argv)
{
	// クラッシュハンドラのテストが自分自身を落とすときの入口（CrashTests.cpp）
	if (argc >= 4 && std::string(argv[1]) == "--crash") { return crashtest::Run(argv[2], argv[3]); }

	SetConsoleOutputCP(CP_UTF8);
	// テスト中の警告などは出さない（失敗の表示が埋もれないように）
	rd::Log::SetLevel(rd::LogLevel::Fatal);

	std::string filter = argc > 1 ? argv[1] : "";
	int passed = 0, failed = 0, checks = 0;
	for (const auto& test : rdtest::Registry())
	{
		std::string fullName = std::string(test.suite) + "." + test.name;
		if (!filter.empty() && fullName.find(filter) == std::string::npos) { continue; }

		rdtest::Context ctx;
		rdtest::Current() = &ctx;
		try
		{
			test.func();
		}
		catch (const rdtest::RequireFailed&)
		{
		}
		catch (const std::exception& e)
		{
			ctx.failures.push_back(std::string("exception: ") + e.what());
		}
		rdtest::Current() = nullptr;
		checks += ctx.checks;

		if (ctx.failures.empty())
		{
			++passed;
			std::printf("  ok    %s\n", fullName.c_str());
		}
		else
		{
			++failed;
			std::printf("  FAIL  %s\n", fullName.c_str());
			for (const auto& f : ctx.failures) { std::printf("        %s\n", f.c_str()); }
		}
	}
	std::printf("\n%d passed, %d failed (%d checks)\n", passed, failed, checks);
	rd::Log::Shutdown();
	return failed;
}
