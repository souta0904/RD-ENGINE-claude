#include "RdPch.h"
#include "Asset/AssetManager.h"
#include "Asset/Model.h"
#include "Asset/Texture.h"
#include "Asset/AudioClip.h"
#include "Core/StringUtil.h"

namespace rd
{
	void AssetManager::Initialize()
	{
		RD_ASSERT(!sInstance);
		sInstance = this;

		// 既定テクスチャ（白・平らな法線）
		const uint32_t white = 0xFFFFFFFF;
		const uint32_t flatNormal = 0xFFFF8080;// (0.5, 0.5, 1.0)
		mWhite = std::make_shared<Texture>();
		mWhite->mPath = "Engine/White";
		mWhite->LoadFromPixels(&white, 1, 1, false);
		mFlatNormal = std::make_shared<Texture>();
		mFlatNormal->mPath = "Engine/FlatNormal";
		mFlatNormal->LoadFromPixels(&flatNormal, 1, 1, false);
		Register(mWhite->mPath, mWhite);
		Register(mFlatNormal->mPath, mFlatNormal);
	}

	void AssetManager::Shutdown()
	{
		// 読み込み中のジョブが終わるのを待つ
		while (mLoadingCount.load() > 0) { std::this_thread::yield(); }
		std::lock_guard lock(mMutex);
		mAssets.clear();
		mWhite.reset();
		mFlatNormal.reset();
		sInstance = nullptr;
	}

	AssetRef<Texture> AssetManager::LoadTexture(const std::string& path, bool srgb)
	{
		if (path.empty()) { return nullptr; }
		std::string key = NormalizePath(path);
		if (!srgb && !EndsWith(key, "#linear")) { key += "#linear"; }
		return Load<Texture>(key);
	}

	AssetRef<Model> AssetManager::LoadModel(const std::string& path)
	{
		return Load<Model>(NormalizePath(path));
	}

	AssetRef<Model> AssetManager::LoadAnimations(const std::string& path)
	{
		if (path.empty()) { return nullptr; }
		return Load<Model>(NormalizePath(path) + "#anim");
	}

	AssetRef<AudioClip> AssetManager::LoadAudio(const std::string& path)
	{
		return Load<AudioClip>(NormalizePath(path));
	}

	void AssetManager::Register(const std::string& key, std::shared_ptr<Asset> asset)
	{
		std::lock_guard lock(mMutex);
		asset->mPath = key;
		mAssets[key] = std::move(asset);
	}

	uint32_t AssetManager::CollectGarbage()
	{
		std::lock_guard lock(mMutex);
		uint32_t count = 0;
		for (auto it = mAssets.begin(); it != mAssets.end();)
		{
			// キャッシュだけが保持していて、読み込みも終わっているもの
			if (it->second.use_count() == 1 && !it->second->IsLoading() && !it->first.starts_with("Engine/"))
			{
				it = mAssets.erase(it);
				++count;
			}
			else
			{
				++it;
			}
		}
		return count;
	}

	void AssetManager::Reload(const std::string& key)
	{
		std::lock_guard lock(mMutex);
		mAssets.erase(key);
	}

	std::vector<std::shared_ptr<Asset>> AssetManager::GetAllAssets()
	{
		std::lock_guard lock(mMutex);
		std::vector<std::shared_ptr<Asset>> result;
		result.reserve(mAssets.size());
		for (auto& [key, asset] : mAssets) { result.push_back(asset); }
		std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a->GetPath() < b->GetPath(); });
		return result;
	}

	void AssetManager::RunLoad(std::shared_ptr<Asset> asset)
	{
		try
		{
			asset->Load();
		}
		catch (const std::exception& e)
		{
			Log::Error(LogCat::Asset, "Exception while loading '{}': {}", asset->GetPath(), e.what());
			asset->SetState(AssetState::Failed);
		}
		--mLoadingCount;
	}
}
