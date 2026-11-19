#include "RdPch.h"
#include "Editor/Core/FileDialog.h"
#include "Core/Util/StringUtil.h"
#include <commdlg.h>
#include <shellapi.h>

#pragma comment(lib, "comdlg32.lib")

namespace rd
{
	std::string FileDialog::Open(HWND owner, const wchar_t* filter, const wchar_t* defaultExt)
	{
		return Show(owner, false, filter, defaultExt);
	}

	std::string FileDialog::Save(HWND owner, const wchar_t* filter, const wchar_t* defaultExt)
	{
		return Show(owner, true, filter, defaultExt);
	}

	std::string FileDialog::Show(HWND owner, bool save, const wchar_t* filter, const wchar_t* defaultExt)
	{
		wchar_t buffer[MAX_PATH] = {};
		OPENFILENAMEW ofn = {};
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = owner;
		ofn.lpstrFilter = filter;
		ofn.lpstrFile = buffer;
		ofn.nMaxFile = MAX_PATH;
		ofn.lpstrDefExt = defaultExt;
		std::wstring initial = std::filesystem::absolute(L"Assets").wstring();
		ofn.lpstrInitialDir = initial.c_str();
		ofn.Flags = OFN_NOCHANGEDIR | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
		BOOL ok = save ? GetSaveFileNameW(&ofn) : GetOpenFileNameW(&ofn);
		if (!ok) { return {}; }
		std::error_code ec;
		auto rel = std::filesystem::relative(buffer, std::filesystem::current_path(), ec);
		return NormalizePath(ToUtf8(ec ? std::wstring(buffer) : rel.wstring()));
	}

	void FileDialog::ShowInExplorer(const std::string& path)
	{
		std::wstring args = L"/select,\"" + std::filesystem::absolute(ToWide(path)).wstring() + L"\"";
		ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
	}

	// ShellExecute は 32 以下を返すと失敗（その拡張子に開くアプリが決まっていないなど）
	void FileDialog::OpenWithDefaultApp(const std::string& path)
	{
		std::wstring file = std::filesystem::absolute(ToWide(path)).wstring();
		auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", file.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
		if (result <= 32)
		{
			std::wstring quoted = L"\"" + file + L"\"";
			ShellExecuteW(nullptr, L"open", L"notepad.exe", quoted.c_str(), nullptr, SW_SHOWNORMAL);
		}
	}

	// SHFileOperation はパスの終わりに \0 が 2 つ要る
	bool FileDialog::MoveToRecycleBin(const std::string& path)
	{
		std::wstring from = std::filesystem::absolute(ToWide(path)).wstring();
		from.push_back(L'\0');
		SHFILEOPSTRUCTW op = {};
		op.wFunc = FO_DELETE;
		op.pFrom = from.c_str();
		op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;
		return SHFileOperationW(&op) == 0 && !op.fAnyOperationsAborted;
	}
}
