#include "RdPch.h"
#include "Core/CrashHandler.h"
#include "Core/StringUtil.h"
#include <DbgHelp.h>
#include <csignal>
#include <fstream>
#include <shellapi.h>
#include <sstream>

#pragma comment(lib, "dbghelp.lib")

namespace rd
{
	namespace
	{
		struct CrashState
		{
			CrashHandler::Settings settings;
			bool installed = false;
			DWORD mainThreadId = 0;
			std::atomic<bool> handling = false;// 2 回目以降のクラッシュ（処理中の abort など）は無視する

			std::mutex contextMutex;
			std::vector<std::pair<std::string, std::function<std::string()>>> contexts;

			std::mutex symMutex;// DbgHelp はスレッドセーフではない
			bool symInitialized = false;
		};

		CrashState& State()
		{
			static CrashState* state = new CrashState();// 終了処理の順番に左右されないよう解放しない
			return *state;
		}

		// 落ちたときの情報（ヘルパースレッドへ渡す）
		struct CrashInfo
		{
			std::string reason;
			CONTEXT context = {};
			EXCEPTION_RECORD record = {};
			bool hasRecord = false;
			DWORD threadId = 0;
			std::string reportPath;
		};

		// --------------------------------------------------
		// シンボル（関数名・ファイル名・行番号）
		// --------------------------------------------------
		void EnsureSymbols()
		{
			CrashState& s = State();
			if (s.symInitialized) { return; }
			SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS);
			// exe と同じ場所の .pdb を探す
			s.symInitialized = SymInitialize(GetCurrentProcess(), nullptr, TRUE) != FALSE;
		}

		std::string DescribeAddress(DWORD64 address)
		{
			HANDLE process = GetCurrentProcess();
			std::string module = "?";
			if (DWORD64 base = SymGetModuleBase64(process, address))
			{
				char path[MAX_PATH] = {};
				if (GetModuleFileNameA(HMODULE(base), path, MAX_PATH))
				{
					module = std::filesystem::path(path).filename().string();
				}
			}

			alignas(SYMBOL_INFO) char buffer[sizeof(SYMBOL_INFO) + 512] = {};
			auto symbol = reinterpret_cast<SYMBOL_INFO*>(buffer);
			symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
			symbol->MaxNameLen = 511;
			DWORD64 displacement = 0;
			std::string text;
			if (SymFromAddr(process, address, &displacement, symbol))
			{
				text = std::format("{}!{}+0x{:X}", module, symbol->Name, displacement);
			}
			else
			{
				text = std::format("{}!0x{:016X}", module, address);
			}

			IMAGEHLP_LINE64 line = {};
			line.SizeOfStruct = sizeof(line);
			DWORD lineDisp = 0;
			if (SymGetLineFromAddr64(process, address, &lineDisp, &line))
			{
				text += std::format("  ({}:{})", line.FileName, line.LineNumber);
			}
			return text;
		}

		std::string WalkStack(HANDLE thread, CONTEXT context, int skip)
		{
			std::lock_guard lock(State().symMutex);
			EnsureSymbols();
			STACKFRAME64 frame = {};
			frame.AddrPC.Offset = context.Rip;
			frame.AddrPC.Mode = AddrModeFlat;
			frame.AddrFrame.Offset = context.Rbp;
			frame.AddrFrame.Mode = AddrModeFlat;
			frame.AddrStack.Offset = context.Rsp;
			frame.AddrStack.Mode = AddrModeFlat;

			std::string out;
			int index = 0;
			for (int i = 0; i < 64; ++i)
			{
				if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, GetCurrentProcess(), thread, &frame, &context, nullptr,
					SymFunctionTableAccess64, SymGetModuleBase64, nullptr))
				{
					break;
				}
				if (frame.AddrPC.Offset == 0) { break; }
				if (i < skip) { continue; }
				out += std::format("  #{:<2} {}\n", index++, DescribeAddress(frame.AddrPC.Offset));
			}
			return out;
		}

		const char* ExceptionName(DWORD code)
		{
			switch (code)
			{
			case EXCEPTION_ACCESS_VIOLATION: return "Access violation";
			case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "Array bounds exceeded";
			case EXCEPTION_DATATYPE_MISALIGNMENT: return "Datatype misalignment";
			case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "Float divide by zero";
			case EXCEPTION_INT_DIVIDE_BY_ZERO: return "Integer divide by zero";
			case EXCEPTION_ILLEGAL_INSTRUCTION: return "Illegal instruction";
			case EXCEPTION_IN_PAGE_ERROR: return "In-page error";
			case EXCEPTION_STACK_OVERFLOW: return "Stack overflow";
			case EXCEPTION_PRIV_INSTRUCTION: return "Privileged instruction";
			case EXCEPTION_BREAKPOINT: return "Breakpoint";
			case 0xE06D7363: return "C++ exception";
			default: return "Exception";
			}
		}

		// --------------------------------------------------
		// MSVC の C++ 例外（0xE06D7363）から std::exception::what() を取り出す
		//  例外レコードには「投げたオブジェクト」と「型の一覧（ThrowInfo）」が入っている。
		//  型の一覧に std::exception があれば、そのオブジェクトを std::exception として読める。
		// --------------------------------------------------
		constexpr DWORD kCppExceptionCode = 0xE06D7363;
		struct MsvcThrowInfo { uint32_t attributes; int32_t unwind; int32_t forwardCompat; int32_t catchableTypes; };
		struct MsvcCatchableTypeArray { int32_t count; int32_t types[1]; };
		struct MsvcCatchableType { uint32_t properties; int32_t typeDescriptor; int32_t mdisp, pdisp, vdisp; int32_t size; int32_t copy; };
		struct MsvcTypeDescriptor { const void* vftable; void* spare; char name[1]; };

		const std::exception* FindStdException(const EXCEPTION_RECORD& record)
		{
			if (record.ExceptionCode != kCppExceptionCode || record.NumberParameters < 4) { return nullptr; }
			auto object = reinterpret_cast<const uint8_t*>(record.ExceptionInformation[1]);
			auto info = reinterpret_cast<const MsvcThrowInfo*>(record.ExceptionInformation[2]);
			auto base = reinterpret_cast<const uint8_t*>(record.ExceptionInformation[3]);// x64 では型情報はモジュール先頭からの相対位置
			if (!object || !info || !base || !info->catchableTypes) { return nullptr; }
			auto array = reinterpret_cast<const MsvcCatchableTypeArray*>(base + info->catchableTypes);
			for (int32_t i = 0; i < array->count; ++i)
			{
				auto type = reinterpret_cast<const MsvcCatchableType*>(base + array->types[i]);
				auto desc = reinterpret_cast<const MsvcTypeDescriptor*>(base + type->typeDescriptor);
				if (std::strcmp(desc->name, ".?AVexception@std@@") == 0)
				{
					return reinterpret_cast<const std::exception*>(object + type->mdisp);
				}
			}
			return nullptr;
		}

		// 壊れたデータを読んでもここで止まるように SEH で囲む（C++ のオブジェクトを持てないので別関数）
		bool CopyExceptionMessage(const EXCEPTION_RECORD& record, char* out, size_t size)
		{
			__try
			{
				const std::exception* e = FindStdException(record);
				if (!e) { return false; }
				strncpy_s(out, size, e->what(), _TRUNCATE);
				return true;
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				return false;
			}
		}

		std::string Timestamp()
		{
			SYSTEMTIME t;
			GetLocalTime(&t);
			return std::format("{:04}{:02}{:02}_{:02}{:02}{:02}", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
		}

		std::string SystemInfo()
		{
			std::string out;
			using RtlGetVersionFunc = LONG(WINAPI*)(OSVERSIONINFOW*);
			if (auto rtlGetVersion = reinterpret_cast<RtlGetVersionFunc>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion")))
			{
				OSVERSIONINFOW v = { sizeof(v) };
				rtlGetVersion(&v);
				out += std::format("OS:           Windows {}.{}.{}\n", v.dwMajorVersion, v.dwMinorVersion, v.dwBuildNumber);
			}
			SYSTEM_INFO si;
			GetNativeSystemInfo(&si);
			out += std::format("CPU threads:  {}\n", si.dwNumberOfProcessors);
			MEMORYSTATUSEX mem = { sizeof(mem) };
			if (GlobalMemoryStatusEx(&mem))
			{
				out += std::format("Memory:       {} MB free / {} MB\n", mem.ullAvailPhys >> 20, mem.ullTotalPhys >> 20);
			}
			out += std::format("Command line: {}\n", ToUtf8(GetCommandLineW()));
			return out;
		}

		// --------------------------------------------------
		// レポートとダンプを書く（落ちたスレッドとは別のスレッドで実行する。
		//  スタックオーバーフローで落ちたスレッドには、もうほとんどスタックが残っていないため）
		// --------------------------------------------------
		void WriteReport(CrashInfo& info)
		{
			CrashState& s = State();
			std::error_code ec;
			std::filesystem::create_directories(ToWide(s.settings.directory), ec);
			std::string stamp = Timestamp();
			std::filesystem::path dir(ToWide(s.settings.directory));
			std::filesystem::path dumpPath = dir / ToWide("Crash_" + stamp + ".dmp");
			std::filesystem::path reportPath = dir / ToWide("Crash_" + stamp + ".txt");

			// ミニダンプ
			bool dumped = false;
			if (s.settings.writeDump)
			{
				HANDLE file = CreateFileW(dumpPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
				if (file != INVALID_HANDLE_VALUE)
				{
					EXCEPTION_POINTERS pointers{ &info.record, &info.context };
					MINIDUMP_EXCEPTION_INFORMATION mei{ info.threadId, &pointers, FALSE };
					std::lock_guard lock(s.symMutex);
					dumped = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file,
						MINIDUMP_TYPE(MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithThreadInfo | MiniDumpWithDataSegs),
						&mei, nullptr, nullptr) != FALSE;
					CloseHandle(file);
				}
			}

			// 呼び出し履歴（落ちたスレッドのレジスタから辿る）
			std::string stack;
			HANDLE thread = OpenThread(THREAD_ALL_ACCESS, FALSE, info.threadId);
			if (thread)
			{
				stack = WalkStack(thread, info.context, 0);
				CloseHandle(thread);
			}
			std::string where;
			{
				std::lock_guard lock(s.symMutex);
				EnsureSymbols();
				where = DescribeAddress(info.context.Rip);
			}

			std::ostringstream r;
			r << "RD ENGINE crash report\n";
			r << "======================\n";
			r << "Time:         " << stamp << "\n";
			r << "Reason:       " << info.reason << "\n";
			r << "Location:     " << where << "\n";
			r << "Thread:       " << info.threadId << (info.threadId == s.mainThreadId ? " (main)" : "") << "\n";
			r << SystemInfo();
			{
				// 落ちた状態なので、ロックが取れないときは飛ばす
				std::unique_lock lock(s.contextMutex, std::try_to_lock);
				if (lock.owns_lock())
				{
					for (auto& [key, provider] : s.contexts)
					{
						std::string value;
						try { value = provider(); }
						catch (...) { value = "(failed)"; }
						r << std::format("{:<14}{}\n", key + ":", value);
					}
				}
			}
			r << "\nCall stack:\n" << (stack.empty() ? "  (unavailable)\n" : stack);
			r << "\nRecent log:\n";
			std::vector<std::string> recent;
			Log::ForEach([&](const LogRecord& rec)
				{
					recent.push_back(std::format("  {:9.3f} {:<5} [{}] {}", rec.time, ToString(rec.level), rec.category, rec.message));
				});
			size_t first = recent.size() > 40 ? recent.size() - 40 : 0;
			for (size_t i = first; i < recent.size(); ++i) { r << recent[i] << "\n"; }
			if (dumped) { r << "\nDump: " << ToUtf8(dumpPath.wstring()) << "  (open in Visual Studio and press \"Debug with Native Only\")\n"; }

			std::ofstream(reportPath, std::ios::trunc) << r.str();
			info.reportPath = ToUtf8(reportPath.wstring());

			Log::Write(LogLevel::Fatal, LogCat::Core, std::source_location::current(),
				std::format("CRASH: {} at {}\nCall stack:\n{}Report: {}", info.reason, where, stack, info.reportPath));
			Log::Flush();
		}

		void ShowDialog(const CrashInfo& info)
		{
			std::wstring text = L"申し訳ありません。エラーが起きたため終了します。\n\n" + ToWide(info.reason) +
				L"\n\nクラッシュレポートを保存しました:\n" + ToWide(info.reportPath) + L"\n\nフォルダを開きますか？";
			if (MessageBoxW(nullptr, text.c_str(), L"RD ENGINE", MB_ICONERROR | MB_YESNO | MB_TOPMOST | MB_SETFOREGROUND) == IDYES)
			{
				std::wstring args = L"/select,\"" + std::filesystem::absolute(ToWide(info.reportPath)).wstring() + L"\"";
				ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
			}
		}

		void HandleCrash(CrashInfo& info)
		{
			CrashState& s = State();
			if (s.handling.exchange(true)) { return; }

			// 落ちたスレッドとは別のスレッドで書く（固まっても 20 秒で諦める）
			HANDLE worker = CreateThread(nullptr, 0, [](void* p) -> DWORD
				{
					WriteReport(*static_cast<CrashInfo*>(p));
					return 0;
				}, &info, 0, nullptr);
			if (worker)
			{
				WaitForSingleObject(worker, 20000);
				CloseHandle(worker);
			}
			if (s.settings.showDialog && !IsDebuggerPresent() && !info.reportPath.empty())
			{
				HANDLE dialog = CreateThread(nullptr, 0, [](void* p) -> DWORD
					{
						ShowDialog(*static_cast<CrashInfo*>(p));
						return 0;
					}, &info, 0, nullptr);
				if (dialog)
				{
					WaitForSingleObject(dialog, INFINITE);
					CloseHandle(dialog);
				}
			}
		}

		// 今の場所でクラッシュとして扱う（abort などの例外ではない終わり方）
		void HandleHere(std::string reason)
		{
			CrashInfo info;
			RtlCaptureContext(&info.context);
			info.record.ExceptionCode = 0xE0000001;// アプリ定義のコード（ダンプの例外情報に入る）
			info.record.ExceptionAddress = reinterpret_cast<void*>(info.context.Rip);
			info.threadId = GetCurrentThreadId();
			info.reason = std::move(reason);
			HandleCrash(info);
		}

		// --------------------------------------------------
		// 各種ハンドラ
		// --------------------------------------------------
		LONG WINAPI OnUnhandledException(EXCEPTION_POINTERS* ep)
		{
			CrashInfo info;
			info.context = *ep->ContextRecord;
			info.record = *ep->ExceptionRecord;
			info.hasRecord = true;
			info.threadId = GetCurrentThreadId();
			DWORD code = ep->ExceptionRecord->ExceptionCode;
			info.reason = std::format("{} (0x{:08X})", ExceptionName(code), code);
			if (char message[512]; code == kCppExceptionCode && CopyExceptionMessage(*ep->ExceptionRecord, message, sizeof(message)))
			{
				info.reason = std::format("Uncaught C++ exception: {}", message);
			}
			if (code == EXCEPTION_ACCESS_VIOLATION && ep->ExceptionRecord->NumberParameters >= 2)
			{
				ULONG_PTR op = ep->ExceptionRecord->ExceptionInformation[0];
				info.reason += std::format(" {} address 0x{:X}", op == 0 ? "reading" : (op == 1 ? "writing" : "executing"),
					ep->ExceptionRecord->ExceptionInformation[1]);
			}
			HandleCrash(info);
			return EXCEPTION_EXECUTE_HANDLER;// Windows のエラー報告ダイアログは出さずに終了する
		}

		void OnAbortSignal(int)
		{
			HandleHere("abort() was called");
		}

		void OnTerminate()
		{
			std::string what = "std::terminate was called";
			if (auto e = std::current_exception())
			{
				try { std::rethrow_exception(e); }
				catch (const std::exception& ex) { what = std::format("Uncaught C++ exception: {}", ex.what()); }
				catch (...) { what = "Uncaught C++ exception (unknown type)"; }
			}
			HandleHere(what);
			TerminateProcess(GetCurrentProcess(), 3);
		}

		void OnPureCall()
		{
			HandleHere("Pure virtual function call");
			TerminateProcess(GetCurrentProcess(), 3);
		}

		void OnInvalidParameter(const wchar_t* expression, const wchar_t* function, const wchar_t*, unsigned int, uintptr_t)
		{
			std::string reason = "Invalid parameter passed to a CRT function";
			if (function) { reason += " (" + ToUtf8(function) + ")"; }
			if (expression) { reason += ": " + ToUtf8(expression); }
			HandleHere(reason);
			TerminateProcess(GetCurrentProcess(), 3);
		}
	}

	void CrashHandler::Install(const Settings& settings)
	{
		CrashState& s = State();
		s.settings = settings;
		if (s.installed) { return; }
		s.installed = true;
		s.mainThreadId = GetCurrentThreadId();

		SetUnhandledExceptionFilter(OnUnhandledException);
		std::set_terminate(OnTerminate);
		std::signal(SIGABRT, OnAbortSignal);
		_set_purecall_handler(OnPureCall);
		_set_invalid_parameter_handler(OnInvalidParameter);
		// abort() の「Debug Error!」ダイアログや Windows のエラー報告を出さない（自前のダイアログを出すので）
		_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
		// スタックオーバーフローでもハンドラが動けるよう、メインスレッドに予備のスタックを確保する
		ULONG guarantee = 64 * 1024;
		SetThreadStackGuarantee(&guarantee);
	}

	bool CrashHandler::IsInstalled() { return State().installed; }

	void CrashHandler::SetContext(const std::string& key, std::function<std::string()> provider)
	{
		CrashState& s = State();
		std::lock_guard lock(s.contextMutex);
		for (auto& [k, p] : s.contexts)
		{
			if (k == key) { p = std::move(provider); return; }
		}
		s.contexts.emplace_back(key, std::move(provider));
	}

	std::string CrashHandler::CaptureStackTrace(int skip)
	{
		CONTEXT context = {};
		RtlCaptureContext(&context);
		// 自分（CaptureStackTrace）の分も飛ばす
		return WalkStack(GetCurrentThread(), context, skip + 1);
	}
}
