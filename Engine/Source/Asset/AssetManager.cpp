#include "RdPch.h"
#include "Asset/AssetManager.h"
#include "Asset/Audio/AudioClip.h"
#include "Asset/Model/Model.h"
#include "Asset/Texture/Texture.h"
#include "Core/Util/StringUtil.h"

namespace rd
{
	void AssetManager::Initialize()
	{
		RD_ASSERT(!sInstance);
		sInstance = this;

		// 平らな法線は (0.5, 0.5, 1.0)
		const uint32_t white = 0xFFFFFFFF;
		const uint32_t flatNormal = 0xFFFF8080;
		mWhite = std::make_shared<Texture>();
		mWhite->mPath = "Engine/White";
		mWhite->LoadFromPixels(&white, 1, 1, false);
		mFlatNormal = std::make_shared<Texture>();
		mFlatNormal->mPath = "Engine/FlatNormal";
		mFlatNormal->LoadFromPixels(&flatNormal, 1, 1, false);
		Register(mWhite->mPath, mWhite);
		Register(mFlatNormal->mPath, mFlatNormal);

		// 自作のシェーダーのテクスチャの既定値（Unity の "black" / "gray" と同じ）
		const uint32_t black = 0xFF000000;
		const uint32_t gray = 0xFF808080;
		mBlack = std::make_shared<Texture>();
		mBlack->mPath = "Engine/Black";
		mBlack->LoadFromPixels(&black, 1, 1, false);
		mGray = std::make_shared<Texture>();
		mGray->mPath = "Engine/Gray";
		mGray->LoadFromPixels(&gray, 1, 1, false);
		Register(mBlack->mPath, mBlack);
		Register(mGray->mPath, mGray);
	}

	AssetRef<Texture> AssetManager::GetBuiltinTexture(const std::string& name) const
	{
		if (name == "black") { return mBlack; }
		if (name == "gray" || name == "grey") { return mGray; }
		if (name == "bump") { return mFlatNormal; }
		return mWhite;
	}

	void AssetManager::Shutdown()
	{
		// 読み込み中のジョブが終わるのを待つ
		while (mLoadingCount.load() > 0) { std::this_thread::yield(); }
		std::lock_guard lock(mMutex);
		mAssets.clear();
		mWhite.reset();
		mFlatNormal.reset();
		mBlack.reset();
		mGray.reset();
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

	// LoadNow（ヘッダーのテンプレート）がキーを作るのに使う。NormalizePath をヘッダーから見せないための窓口
	std::string AssetManager::NormalizePathKey(const std::string& path)
	{
		return NormalizePath(path);
	}

	void AssetManager::Register(const std::string& key, std::shared_ptr<Asset> asset)
	{
		std::lock_guard lock(mMutex);
		asset->mPath = key;
		mAssets[key] = std::move(asset);
	}

	// キャッシュだけが持っていて、読み込みも終わっているものを消す（Engine/ の組み込みは残す）
	uint32_t AssetManager::CollectGarbage()
	{
		std::lock_guard lock(mMutex);
		uint32_t count = 0;
		for (auto it = mAssets.begin(); it != mAssets.end();)
		{
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
