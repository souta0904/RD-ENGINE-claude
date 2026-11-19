#pragma once
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace rd
{
	// プロジェクトの外や中のファイルを Assets へ取り込む
	//  fbx などのモデルは .rdmodel に変換し、元のファイルは取り込み先へコピーしない（元の場所のファイルも変更しない）。
	//  モデルが使うテクスチャは .rdmodel の隣へ置く（プロジェクトの中にあるものはコピーせず、そのまま参照する）。
	//  それ以外のファイルはコピーする。フォルダは中身ごと取り込む。同じ名前があれば上書きせず、別の名前にする
	class AssetImporter
	{
	public:
		// ワーカースレッドで取り込む（大きなモデルでもエディタが止まらない）
		static void ImportAsync(std::vector<std::filesystem::path> sources, std::string folder);
		// 今のスレッドで取り込む。変換・コピーできた数を返す
		static uint32_t Import(const std::vector<std::filesystem::path>& sources, const std::string& folder);

		// 取り込み中の要求の数
		static uint32_t GetPendingCount() { return sPending.load(); }

	private:
		static inline std::atomic<uint32_t> sPending = 0;
	};
}
