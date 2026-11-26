#pragma once
#include "Editor/Preview/PreviewView.h"
#include <memory>
#include <string>
#include <unordered_map>

namespace rd
{
	// プロジェクトパネルのモデル・プレハブ・マテリアルのサムネイル（1 フレームに少しずつ描いて覚えておく）
	class RD_API AssetThumbnails
	{
	public:
		AssetThumbnails() = default;
		// RD_API で DLL から出すと、コピーできないメンバーのコピーまで作ろうとするので禁止しておく
		AssetThumbnails(const AssetThumbnails&) = delete;
		AssetThumbnails& operator=(const AssetThumbnails&) = delete;

		// まだ描けていなければ nullptr（そのときは描く予約だけする）
		const GpuTexture* Get(const std::string& path);
		// プレハブのサムネイルを捨てる（次に見えたときに描き直す。ゲームのコードを読み直したとき）
		void ForgetPrefabs();

	private:
		struct Entry
		{
			std::unique_ptr<PreviewView> view;
			bool rendered = false;
			bool done = false;
			int lastUsed = 0;
		};
		// しばらく見ていないものを捨てる
		void Evict();

		std::unordered_map<std::string, Entry> mEntries;
		int mFrame = -1;
		int mRenderedThisFrame = 0;
	};
}
