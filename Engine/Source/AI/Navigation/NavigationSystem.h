#pragma once
#include "AI/Navigation/NavMesh.h"
#include "Scene/SceneSystem.h"
#include <unordered_map>

namespace rd
{
	// シーンのナビメッシュ（scene.GetSystem<NavigationSystem>()）
	//  NavMeshSurface が形を読み込み、NavMeshLink がリンクを登録する。どちらが先でもよいように、
	//  リンクは覚えておいて、使うときに形が変わっていればつなぎ直す。
	class NavigationSystem final : public SceneSystem
	{
	public:
		// 同じパスは 1 回だけ読む（焼き直してファイルが変わったときは reload で読み直す）
		bool LoadSurface(const std::string& path, bool reload = false)
		{
			if (path == mLoadedPath && !reload) { return !mMesh.IsEmpty(); }
			mLoadedPath = path;
			bool ok = mMesh.LoadJson(path);
			mLinksDirty = true;
			return ok;
		}

		// 形を捨てる（リンクは覚えたまま。次に形を読んだらつなぎ直す）
		void ClearSurface()
		{
			mMesh.Clear();
			mLoadedPath.clear();
			mLinksDirty = true;
		}

		void SetLink(const void* owner, const NavMesh::Link& link)
		{
			mLinks[owner] = link;
			mLinksDirty = true;
		}

		void RemoveLink(const void* owner)
		{
			if (mLinks.erase(owner) > 0) { mLinksDirty = true; }
		}

		NavMesh& GetMesh()
		{
			if (mLinksDirty)
			{
				mLinksDirty = false;
				mMesh.ClearLinks();
				for (const auto& [owner, link] : mLinks) { mMesh.AddLink(link); }
			}
			return mMesh;
		}

	private:
		NavMesh mMesh;
		std::string mLoadedPath;
		std::unordered_map<const void*, NavMesh::Link> mLinks;
		bool mLinksDirty = false;
	};
}
