#include "TestFramework.h"
#include "Core/Assert.h"
#include "Core/CrashHandler.h"
#include "Core/StringUtil.h"
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

// クラッシュハンドラのテスト
//  自分自身（Tests.exe）を --crash <種類> <出力先> で起動してわざと落とし、レポートができたかを調べる

namespace crashtest
{
	// 最適化で消えないように、外から見える関数にしておく
	__declspec(noinline) void CrashByNullWrite()
	{
		volatile int* p = nullptr;
		*p = 42;
	}

#pragma warning(suppress : 4717)// わざと無限に再帰する
	__declspec(noinline) int Recurse(int n)
	{
		volatile char buffer[1024];
		buffer[0] = char(n);
		return Recurse(n + 1) + buffer[0];
	}

	struct Base
	{
		Base() { Call(); }
		virtual ~Base() = default;
		void Call() { Pure(); }// 構築中なので純粋仮想関数が呼ばれる
		virtual void Pure() = 0;
	};
	struct Derived : Base
	{
		void Pure() override {}
	};

	// Tests.exe --crash から呼ばれる。戻らない
	int Run(const std::string& kind, const std::string& dir)
	{
		rd::CrashHandler::Install({ .directory = dir, .showDialog = false, .writeDump = true });
		if (kind == "access") { CrashByNullWrite(); }
		else if (kind == "verify") { RD_VERIFY(1 + 1 == 3, "math is broken: {}", 42); }
		else if (kind == "exception") { throw std::runtime_error("boom from test"); }
		else if (kind == "stack") { return Recurse(0); }
		else if (kind == "purecall") { Derived d; }
		return 0;
	}
}

namespace
{
	struct ChildResult
	{
		DWORD exitCode = 0;
		std::string report;
		bool hasDump = false;
	};

	ChildResult RunCrash(const std::string& kind)
	{
		namespace fs = std::filesystem;
		fs::path dir = fs::temp_directory_path() / L"RdEngineCrashTest" / rd::ToWide(kind);
		std::error_code ec;
		fs::remove_all(dir, ec);
		fs::create_directories(dir, ec);

		wchar_t exe[MAX_PATH];
		GetModuleFileNameW(nullptr, exe, MAX_PATH);
		std::wstring cmd = L"\"" + std::wstring(exe) + L"\" --crash " + rd::ToWide(kind) + L" \"" + dir.wstring() + L"\"";
		STARTUPINFOW si = { sizeof(si) };
		PROCESS_INFORMATION pi = {};
		ChildResult result;
		// エラーダイアログを出させない
		UINT oldMode = SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
		BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
		SetErrorMode(oldMode);
		if (!ok) { return result; }
		WaitForSingleObject(pi.hProcess, 60000);
		GetExitCodeProcess(pi.hProcess, &result.exitCode);
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);

		for (auto& e : fs::directory_iterator(dir, ec))
		{
			if (e.path().extension() == L".txt")
			{
				std::ifstream in(e.path());
				std::stringstream ss;
				ss << in.rdbuf();
				result.report = ss.str();
			}
			if (e.path().extension() == L".dmp" && e.file_size() > 0) { result.hasDump = true; }
		}
		return result;
	}

	bool Has(const std::string& text, const char* s) { return text.find(s) != std::string::npos; }
}

RD_TEST(Crash, AccessViolationWritesReportWithStack)
{
	auto r = RunCrash("access");
	RD_CHECK(r.exitCode != 0);
	RD_REQUIRE(!r.report.empty());
	RD_CHECK(Has(r.report, "Access violation"));
	RD_CHECK(Has(r.report, "writing address 0x0"));
	RD_CHECK(Has(r.report, "CrashByNullWrite"));// 呼び出し履歴に関数名が出る
	RD_CHECK(Has(r.report, "CrashTests.cpp"));// ファイル名と行も出る
	RD_CHECK(r.hasDump);
}

RD_TEST(Crash, VerifyFailure)
{
	auto r = RunCrash("verify");
	RD_CHECK(r.exitCode != 0);
	RD_REQUIRE(!r.report.empty());
	RD_CHECK(Has(r.report, "abort() was called"));
	RD_CHECK(Has(r.report, "math is broken: 42"));// 直前のログ（アサートの内容）が載る
}

RD_TEST(Crash, UncaughtCppException)
{
	auto r = RunCrash("exception");
	RD_CHECK(r.exitCode != 0);
	RD_REQUIRE(!r.report.empty());
	RD_CHECK(Has(r.report, "boom from test"));
}

RD_TEST(Crash, StackOverflow)
{
	auto r = RunCrash("stack");
	RD_CHECK(r.exitCode != 0);
	RD_REQUIRE(!r.report.empty());
	RD_CHECK(Has(r.report, "Stack overflow"));
	RD_CHECK(Has(r.report, "Recurse"));
}

RD_TEST(Crash, PureVirtualCall)
{
	auto r = RunCrash("purecall");
	RD_CHECK(r.exitCode != 0);
	RD_REQUIRE(!r.report.empty());
	RD_CHECK(Has(r.report, "Pure virtual function call"));
}

RD_TEST(Crash, CaptureStackTraceHere)
{
	std::string stack = rd::CrashHandler::CaptureStackTrace();
	RD_CHECK(Has(stack, "Test_Crash_CaptureStackTraceHere"));
}
