#pragma once
#include <atomic>
#include <memory>
#include <string>

namespace rd
{
	enum class AssetState : uint8_t
	{
		Loading,// 読み込み中（ワーカースレッド or GPU 転送待ち）
		Ready,
		Failed
	};

	// 全アセットの基底クラス
	//  AssetManager::Load<T>() は即座にハンドルを返し、読み込みはバックグラウンドで進む。
	//  利用側は IsReady() を確認してから使う（未完了ならスキップ or 代替表示）。
	class Asset : public std::enable_shared_from_this<Asset>
	{
	public:
		virtual ~Asset() = default;

		const std::string& GetPath() const { return mPath; }
		AssetState GetState() const { return mState.load(std::memory_order_acquire); }
		bool IsReady() const { return GetState() == AssetState::Ready; }
		bool IsLoading() const { return GetState() == AssetState::Loading; }
		bool IsFailed() const { return GetState() == AssetState::Failed; }
		virtual const char* GetTypeName() const = 0;

	protected:
		friend class AssetManager;
		// ワーカースレッドで呼ばれる。完了時に SetState(Ready/Failed) すること
		virtual void Load() = 0;
		void SetState(AssetState state) { mState.store(state, std::memory_order_release); }

		std::string mPath;// キャッシュのキー（"#..." の付加情報を含む）

	private:
		std::atomic<AssetState> mState = AssetState::Loading;
	};

	template <class T>
	using AssetRef = std::shared_ptr<T>;
}
