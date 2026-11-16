#pragma once
#include "Asset/Asset.h"
#include "Core/Jobs/JobSystem.h"
#include <mutex>
#include <unordered_map>
#include <vector>

namespace rd
{
	class Texture;
	class Model;
	class AudioClip;

	// アセットのキャッシュと非同期の読み込み（同じキーなら同じインスタンスを返す）
	class AssetManager
	{
	public:
		static AssetManager& Get() { return *sInstance; }

		void Initialize();
		void Shutdown();

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
			JobSystem::Schedule([this, asset] { RunLoad(asset); }, JobPriority::Low, "Asset Load");
			return asset;
		}

		AssetRef<Texture> LoadTexture(const std::string& path, bool srgb = true);
		AssetRef<Model> LoadModel(const std::string& path);
		// メッシュを作らず、アニメーションだけを読む
		AssetRef<Model> LoadAnimations(const std::string& path);
		AssetRef<AudioClip> LoadAudio(const std::string& path);

		// 読み込み済みのアセットを登録する（モデルに埋め込まれたテクスチャなど）
		void Register(const std::string& key, std::shared_ptr<Asset> asset);
		// どこからも使われていないアセットを解放する
		uint32_t CollectGarbage();
		void Reload(const std::string& key);

		uint32_t GetLoadingCount() const { return mLoadingCount.load(); }
		std::vector<std::shared_ptr<Asset>> GetAllAssets();

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
