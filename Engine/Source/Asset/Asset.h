#pragma once
#include <atomic>
#include <memory>
#include <string>

namespace rd
{
	// Loading はワーカースレッドで読み込み中か、GPU への転送待ち
	enum class AssetState : uint8_t
	{
		Loading,
		Ready,
		Failed
	};

	// すべてのアセットの基底
	//  AssetManager::Load<T>() はすぐにハンドルを返し、読み込みは裏で進む。
	//  使う側は IsReady() を確かめてから使う（まだなら飛ばすか代わりのものを出す）。
	//  mPath はキャッシュのキー（"#linear" などの付加情報を含む）
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
		// ワーカースレッドで呼ばれる。終わったら SetState(Ready / Failed) すること
		virtual void Load() = 0;
		void SetState(AssetState state) { mState.store(state, std::memory_order_release); }

		std::string mPath;

	private:
		std::atomic<AssetState> mState = AssetState::Loading;
	};

	template <class T>
	using AssetRef = std::shared_ptr<T>;
}
