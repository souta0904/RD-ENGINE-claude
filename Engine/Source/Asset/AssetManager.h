#pragma once
#include "Asset/Asset.h"
#include "Core/JobSystem.h"
#include <mutex>
#include <unordered_map>
#include <vector>

namespace rd
{
	class Texture;
	class Model;
	class AudioClip;

	// アセットのキャッシュと非同期読み込み
	class AssetManager
	{
	public:
		static AssetManager& Get() { return *sInstance; }

		void Initialize();
		void Shutdown();

		// 非同期読み込み（同じキーなら同じインスタンスを返す）
		template <class T>
		AssetRef<T> Load(const std::string& key)
		{
			if (key.empty()) { return nullptr; }
			std::lock_guard lock(mMutex);
			if (auto it = mAssets.find(key); it != mAssets.end())
			{
				return std::dynamic_pointer_cast<T>(it->second);
			}
			auto asset = std::make_shared<T>();
			asset->mPath = key;
			mAssets.emplace(key, asset);
			++mLoadingCount;
			JobSystem::Schedule([this, asset] { RunLoad(asset); }, JobPriority::Low);
			return asset;
		}

		// 便利関数
		AssetRef<Texture> LoadTexture(const std::string& path, bool srgb = true);
		AssetRef<Model> LoadModel(const std::string& path);
		AssetRef<Model> LoadAnimations(const std::string& path);// メッシュを作らずアニメーションだけ読む
		AssetRef<AudioClip> LoadAudio(const std::string& path);

		// 読み込み済みのアセットを登録（モデル埋め込みテクスチャなど）
		void Register(const std::string& key, std::shared_ptr<Asset> asset);
		// どこからも参照されていないアセットを解放
		uint32_t CollectGarbage();
		// アセットを読み直す（ファイル更新時）
		void Reload(const std::string& key);

		uint32_t GetLoadingCount() const { return mLoadingCount.load(); }
		std::vector<std::shared_ptr<Asset>> GetAllAssets();

		// 既定のテクスチャ
		AssetRef<Texture> GetWhiteTexture() const { return mWhite; }
		AssetRef<Texture> GetFlatNormalTexture() const { return mFlatNormal; }

	private:
		void RunLoad(std::shared_ptr<Asset> asset);

		static inline AssetManager* sInstance = nullptr;
		std::mutex mMutex;
		std::unordered_map<std::string, std::shared_ptr<Asset>> mAssets;
		std::atomic<uint32_t> mLoadingCount = 0;
		AssetRef<Texture> mWhite;
		AssetRef<Texture> mFlatNormal;
	};
}
