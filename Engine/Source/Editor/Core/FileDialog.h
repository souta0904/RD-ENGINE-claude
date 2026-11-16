#pragma once
#include <Windows.h>
#include <string>

namespace rd
{
	// Win32 のファイルを開く / 保存するダイアログ（今のフォルダからの相対パスを返す。取り消したら空）
	//  filter は L"Scene (*.scene)\0*.scene\0" の形、defaultExt は拡張子（点なし）
	class FileDialog
	{
	public:
		static std::string Open(HWND owner, const wchar_t* filter, const wchar_t* defaultExt);
		static std::string Save(HWND owner, const wchar_t* filter, const wchar_t* defaultExt);
		// エクスプローラーで開いて、そのファイルを選んだ状態にする
		static void ShowInExplorer(const std::string& path);
		// ごみ箱へ移す（元に戻せる）
		static bool MoveToRecycleBin(const std::string& path);

	private:
		static std::string Show(HWND owner, bool save, const wchar_t* filter, const wchar_t* defaultExt);
	};
}
