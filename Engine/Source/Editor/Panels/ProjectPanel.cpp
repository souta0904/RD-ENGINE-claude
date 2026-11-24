#include "RdPch.h"
#include "Editor/Panels/ProjectPanel.h"
#include "Asset/AssetManager.h"
#include "Asset/Model/Model.h"
#include "Asset/Texture/Texture.h"
#include "Core/Util/StringUtil.h"
#include "Editor/Core/AssetImporter.h"
#include "Editor/Core/FileDialog.h"
#include "Editor/Editor.h"
#include "Editor/Style/EditorGui.h"
#include "Editor/Style/EditorStyle.h"
#include "Engine.h"
#include "Render/Material/MaterialAsset.h"
#include "Render/Material/ShaderAsset.h"
#include "Scene/Scene.h"
#include "Editor/Core/EditorLocale.h"
#include <ImGui/imgui_stdlib.h>

namespace rd
{
	namespace
	{
		namespace fs = std::filesystem;

		bool IsInside(const std::string& path, const std::string& folder)
		{
			return path == folder || path.starts_with(folder + "/");
		}

		// ファイルを別のフォルダへ動かす（同じ名前があれば動かさない）
		void MoveInto(const std::string& path, const std::string& folder)
		{
			if (IsInside(folder, path) || GetDirectory(path) == folder + "/") { return; }
			std::string target = folder + "/" + GetFileName(path);
			std::error_code ec;
			if (fs::exists(ToWide(target), ec))
			{
				Logger::Warning(LogCat::Editor, "Cannot move: {} already exists", target);
				return;
			}
			fs::rename(ToWide(path), ToWide(target), ec);
			if (ec) { Logger::Error(LogCat::Editor, "Failed to move {}: {}", path, ec.message()); }
			else { Logger::Info(LogCat::Editor, "Moved {} -> {}", path, target); }
		}
	}

	ProjectPanel::ProjectPanel(Editor& editor) : EditorPanel(editor, "Project") {}

	void ProjectPanel::RestoreState(const std::string& folder, const std::string& selected, float iconScale)
	{
		std::error_code ec;
		if (IsInside(folder, "Assets") && fs::is_directory(ToWide(folder), ec)) { mFolder = folder; }
		if (IsInside(selected, "Assets") && fs::is_regular_file(ToWide(selected), ec)) { mSelected = selected; }
		mIconScale = iconScale;
	}

	// 落とされたファイルは次のフレームの描画でどのフォルダの上かを調べ、その後のここで取り込みを始める
	//  （パネルが隠れていて描画されないときは、今のフォルダへ取り込む）
	void ProjectPanel::Update(float /*dt*/)
	{
		if (!mDroppedFiles.empty())
		{
			AssetImporter::ImportAsync(std::move(mDroppedFiles), mDropFolder.empty() ? mFolder : mDropFolder);
			mDroppedFiles.clear();
			mDropFolder.clear();
		}
		POINT point = {};
		for (auto& file : mEditor.GetEngine().GetWindow().TakeDroppedFiles(point)) { mDroppedFiles.emplace_back(std::move(file)); }
		mDropPoint = ImVec2(float(point.x), float(point.y));
	}

	void ProjectPanel::CheckFileDropTarget(const std::string& folder)
	{
		if (mDroppedFiles.empty()) { return; }
		// スクロールで隠れた項目に当たらないよう、今のウィンドウの範囲の中だけを見る
		ImVec2 windowMin = ImGui::GetWindowPos();
		ImVec2 windowSize = ImGui::GetWindowSize();
		ImVec2 min = ImGui::GetItemRectMin();
		ImVec2 max = ImGui::GetItemRectMax();
		const ImVec2& p = mDropPoint;
		bool inWindow = p.x >= windowMin.x && p.y >= windowMin.y && p.x < windowMin.x + windowSize.x && p.y < windowMin.y + windowSize.y;
		if (inWindow && p.x >= min.x && p.y >= min.y && p.x < max.x && p.y < max.y) { mDropFolder = folder; }
	}

	void ProjectPanel::OnGui()
	{
		mPrefabActor = 0;
		DrawToolbar();
		CollectEntries();

		float treeWidth = std::max(ImGui::GetFontSize() * 12.0f, ImGui::GetContentRegionAvail().x * 0.2f);
		ImGui::BeginChild("folders", ImVec2(treeWidth, 0.0f), ImGuiChildFlags_ResizeX);
		DrawFolderTree(fs::path(L"Assets"), "Assets");
		ImGui::EndChild();
		ImGui::SameLine();
		ImGui::BeginChild("files");
		DrawGrid();
		ImGui::EndChild();
		DrawDialogs();

		if (mPrefabActor != 0) { mEditor.CreatePrefab(mEditor.GetScene().FindActor(mPrefabActor), mPrefabFolder); }
	}

	// 今のフォルダまでのパンくず（押すとそのフォルダへ戻る）と、検索・アイコンの大きさ
	void ProjectPanel::DrawToolbar()
	{
		EditorGui::BeginToolbar("toolbar");
		if (EditorGui::IconButton("##create", Icon::Plus, Tr("Create"))) { ImGui::OpenPopup("CreateAsset"); }
		if (ImGui::BeginPopup("CreateAsset"))
		{
			if (ImGui::MenuItem(Tr("Folder"))) { CreateFolder(); }
			if (ImGui::MenuItem(Tr("Material"))) { CreateMaterial(); }
			if (ImGui::MenuItem(Tr("Shader"))) { CreateShader(); }
			if (ImGui::MenuItem(Tr("Prefab from Selection"), nullptr, false, mEditor.GetSelected() != nullptr))
			{
				mPrefabActor = mEditor.GetSelectedId();
				mPrefabFolder = mFolder;
			}
			ImGui::EndPopup();
		}
		ImGui::SameLine();
		auto parts = Split(mFolder, '/');
		std::string accum;
		for (size_t i = 0; i < parts.size(); ++i)
		{
			accum += (i ? "/" : "") + parts[i];
			if (i) { ImGui::SameLine(0.0f, 2.0f); ImGui::TextDisabled(">"); ImGui::SameLine(0.0f, 2.0f); }
			else { ImGui::SameLine(); }
			ImGui::PushID(int(i));
			if (ImGui::Button(parts[i].c_str())) { mFolder = accum; }
			ImGui::PopID();
		}
		if (AssetImporter::GetPendingCount() > 0)
		{
			ImGui::SameLine();
			ImGui::AlignTextToFramePadding();
			ImGui::TextColored(EditorStyle::kWarning, "%s", Tr("Importing..."));
		}
		float right = 180.0f + 90.0f + ImGui::GetStyle().ItemSpacing.x * 2.0f;
		ImGui::SameLine(std::max(ImGui::GetCursorPosX() + 8.0f, ImGui::GetWindowContentRegionMax().x - right));
		EditorGui::SearchField("##filter", mFilter, Tr("Search"), 180.0f);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(ImGui::GetFontSize() * 5.0f);
		ImGui::SliderFloat("##size", &mIconScale, 0.6f, 2.0f, "");
		if (ImGui::IsItemHovered()) { ImGui::SetTooltip("%s", Tr("Icon size")); }
		EditorGui::EndToolbar();
	}

	void ProjectPanel::CollectEntries()
	{
		mEntries.clear();
		std::error_code ec;
		if (!fs::is_directory(ToWide(mFolder), ec)) { mFolder = "Assets"; }
		std::vector<Entry> files;
		for (auto& e : fs::directory_iterator(ToWide(mFolder), ec))
		{
			Entry entry;
			entry.name = ToUtf8(e.path().filename().wstring());
			if (!mFilter.empty() && !ContainsNoCase(entry.name, mFilter)) { continue; }
			entry.path = mFolder + "/" + entry.name;
			entry.directory = e.is_directory();
			entry.ext = entry.directory ? std::string() : GetExtension(entry.name);
			(entry.directory ? mEntries : files).push_back(std::move(entry));
		}
		mEntries.insert(mEntries.end(), files.begin(), files.end());
	}

	// フォルダの木（アクターやファイルをフォルダの上に落とせる）
	void ProjectPanel::DrawFolderTree(const fs::path& dir, const std::string& path)
	{
		std::error_code ec;
		bool hasChildren = false;
		for (auto& e : fs::directory_iterator(dir, ec)) { if (e.is_directory()) { hasChildren = true; break; } }

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding;
		if (!hasChildren) { flags |= ImGuiTreeNodeFlags_Leaf; }
		if (path == mFolder) { flags |= ImGuiTreeNodeFlags_Selected; }
		if (IsInside(mFolder, path) && path != mFolder) { ImGui::SetNextItemOpen(true, ImGuiCond_Once); }
		if (path == "Assets") { flags |= ImGuiTreeNodeFlags_DefaultOpen; }
		std::string label = "      " + GetFileName(path);
		bool open = ImGui::TreeNodeEx(path.c_str(), flags, "%s", label.c_str());
		CheckFileDropTarget(path);
		ImVec2 min = ImGui::GetItemRectMin();
		float s = EditorStyle::IconSize();
		EditorIcons::Draw(ImGui::GetWindowDrawList(), ImVec2(min.x + ImGui::GetTreeNodeToLabelSpacing(), min.y + (ImGui::GetFrameHeight() - s) * 0.5f), s, Icon::Folder);
		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) { mFolder = path; mSelected.clear(); }
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("ACTOR_ID")) { mPrefabActor = *static_cast<const ActorId*>(p->Data); mPrefabFolder = path; }
			if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("ASSET_PATH")) { MoveInto(static_cast<const char*>(p->Data), path); }
			ImGui::EndDragDropTarget();
		}
		if (open)
		{
			for (auto& e : fs::directory_iterator(dir, ec))
			{
				if (e.is_directory()) { DrawFolderTree(e.path(), path + "/" + ToUtf8(e.path().filename().wstring())); }
			}
			ImGui::TreePop();
		}
	}

	void ProjectPanel::DrawGrid()
	{
		const float cell = 84.0f * mIconScale;
		const float spacing = ImGui::GetStyle().ItemSpacing.x;
		int columns = std::max(1, int((ImGui::GetContentRegionAvail().x + spacing) / (cell + spacing)));
		for (size_t i = 0; i < mEntries.size(); ++i)
		{
			if (i % size_t(columns) != 0) { ImGui::SameLine(); }
			DrawEntry(mEntries[i], cell);
		}
		if (mEntries.empty()) { EditorGui::EmptyHint(Tr(mFilter.empty() ? "This folder is empty." : "No results.")); }

		// 空いている所: 選択を外す、アクターのドロップでプレハブを作る、右クリックのメニュー
		ImVec2 avail = ImGui::GetContentRegionAvail();
		ImGui::InvisibleButton("##background", ImVec2(std::max(1.0f, avail.x), std::max(cell * 0.5f, avail.y)));
		if (ImGui::IsItemClicked()) { mSelected.clear(); }
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("ACTOR_ID")) { mPrefabActor = *static_cast<const ActorId*>(p->Data); mPrefabFolder = mFolder; }
			ImGui::EndDragDropTarget();
		}
		DrawBackgroundMenu();
	}

	// 四角いマスに大きなアイコン（画像は縮小した絵）と名前を描く
	void ProjectPanel::DrawEntry(const Entry& e, float cell)
	{
		ImGui::PushID(e.path.c_str());
		const float textH = ImGui::GetTextLineHeight() * 2.0f;
		ImVec2 p = ImGui::GetCursorScreenPos();
		ImGui::InvisibleButton("##item", ImVec2(cell, cell + textH));
		if (e.directory) { CheckFileDropTarget(e.path); }
		bool hovered = ImGui::IsItemHovered();
		bool selected = e.path == mSelected;
		ImDrawList* dl = ImGui::GetWindowDrawList();
		if (selected || hovered)
		{
			ImVec4 bg = selected ? EditorStyle::kAccentDim : ImGui::GetStyleColorVec4(ImGuiCol_HeaderHovered);
			dl->AddRectFilled(p, ImVec2(p.x + cell, p.y + cell + textH), ImGui::ColorConvertFloat4ToU32(bg), 4.0f);
		}

		float pad = cell * 0.14f;
		float iconSize = cell - pad * 2.0f;
		Icon icon = EditorIcons::ForFile(e.ext, e.directory);
		bool drewThumb = false;
		if (icon == Icon::Image)
		{
			auto tex = AssetManager::Get().LoadTexture(e.path);
			if (tex->IsReady())
			{
				dl->AddImage(ImTextureID(tex->GetSrv().ptr), ImVec2(p.x + pad, p.y + pad), ImVec2(p.x + pad + iconSize, p.y + pad + iconSize));
				drewThumb = true;
			}
		}
		else if (icon == Icon::Model || icon == Icon::Prefab || icon == Icon::Material)
		{
			if (const GpuTexture* thumb = mThumbnails.Get(e.path))
			{
				dl->AddImage(ImTextureID(thumb->GetGpuSrv().ptr), ImVec2(p.x + pad, p.y + pad), ImVec2(p.x + pad + iconSize, p.y + pad + iconSize));
				drewThumb = true;
			}
		}
		if (!drewThumb) { EditorIcons::Draw(dl, ImVec2(p.x + pad, p.y + pad), iconSize, icon); }

		ImVec4 clip(p.x + 2.0f, p.y + cell, p.x + cell - 2.0f, p.y + cell + textH);
		ImVec2 textSize = ImGui::CalcTextSize(e.name.c_str(), nullptr, false, cell - 4.0f);
		float textX = p.x + std::max(2.0f, (cell - textSize.x) * 0.5f);
		dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(textX, p.y + cell), ImGui::GetColorU32(ImGuiCol_Text), e.name.c_str(), nullptr, cell - 4.0f, &clip);

		if (ImGui::IsItemClicked()) { mSelected = e.directory ? std::string() : e.path; }
		if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) { OpenEntry(e); }
		if (hovered) { ImGui::SetTooltip("%s", e.path.c_str()); }
		if (ImGui::BeginDragDropSource())
		{
			ImGui::SetDragDropPayload("ASSET_PATH", e.path.c_str(), e.path.size() + 1);
			EditorIcons::Inline(icon);
			ImGui::SameLine();
			ImGui::TextUnformatted(e.name.c_str());
			ImGui::EndDragDropSource();
		}
		if (e.directory && ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("ACTOR_ID")) { mPrefabActor = *static_cast<const ActorId*>(pl->Data); mPrefabFolder = e.path; }
			if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("ASSET_PATH")) { MoveInto(static_cast<const char*>(pl->Data), e.path); }
			ImGui::EndDragDropTarget();
		}
		DrawEntryMenu(e);
		ImGui::PopID();
	}

	void ProjectPanel::DrawEntryMenu(const Entry& e)
	{
		if (!ImGui::BeginPopupContextItem("item")) { return; }
		if (ImGui::MenuItem(Tr(e.directory ? "Open" : (e.ext == ".scene" ? "Open Scene" : "Select")))) { OpenEntry(e); }
		if (!e.directory && Model::IsSourceExtension(e.ext) && ImGui::MenuItem(Tr("Convert to .rdmodel")))
		{
			AssetImporter::ImportAsync({ std::filesystem::path(ToWide(e.path)) }, mFolder);
		}
		if (ImGui::MenuItem(Tr("Rename"))) { BeginRename(e); }
		if (ImGui::MenuItem(Tr("Delete")))
		{
			mDeletePath = e.path;
			mOpenDelete = true;
		}
		ImGui::Separator();
		if (ImGui::MenuItem(Tr("Show in Explorer"))) { FileDialog::ShowInExplorer(e.path); }
		ImGui::EndPopup();
	}

	void ProjectPanel::DrawBackgroundMenu()
	{
		if (!ImGui::BeginPopupContextItem("##backgroundMenu")) { return; }
		if (ImGui::MenuItem(Tr("Create Folder"))) { CreateFolder(); }
		if (ImGui::MenuItem(Tr("Create Material"))) { CreateMaterial(); }
		if (ImGui::MenuItem(Tr("Create Shader"))) { CreateShader(); }
		if (ImGui::MenuItem(Tr("Create Prefab from Selection"), nullptr, false, mEditor.GetSelected() != nullptr))
		{
			mPrefabActor = mEditor.GetSelectedId();
			mPrefabFolder = mFolder;
		}
		ImGui::Separator();
		if (ImGui::MenuItem(Tr("Show in Explorer"))) { FileDialog::ShowInExplorer(mFolder); }
		ImGui::EndPopup();
	}

	void ProjectPanel::OpenEntry(const Entry& e)
	{
		if (e.directory)
		{
			mFolder = e.path;
			mSelected.clear();
		}
		else if (e.ext == ".scene") { mEditor.OpenScene(e.path); }
		// シェーダーはエディタの中では書けないので、Windows で決めてあるアプリで開く（Unity と同じ）
		else if (e.ext == ".hlsl" || e.ext == ".hlsli") { mSelected = e.path; FileDialog::OpenWithDefaultApp(e.path); }
		else { mSelected = e.path; mEditor.Select(nullptr); }
	}

	void ProjectPanel::SelectFile(const std::string& path)
	{
		std::string folder = GetDirectory(path);
		if (!folder.empty() && folder.back() == '/') { folder.pop_back(); }
		std::error_code ec;
		if (folder.empty() || !fs::is_directory(ToWide(folder), ec)) { return; }
		mFolder = folder;
		mSelected = path;
	}

	// 今のフォルダに "New Material.rdmat" を作り、選んで名前の変更を始める（Unity の Create > Material）
	void ProjectPanel::CreateMaterial()
	{
		std::string path = std::format("{}/New Material{}", mFolder, MaterialAsset::kExtension);
		for (int n = 1; fs::exists(ToWide(path)); ++n) { path = std::format("{}/New Material {}{}", mFolder, n, MaterialAsset::kExtension); }
		if (!MaterialAsset::CreateFile(path)) { return; }
		Logger::Info(LogCat::Editor, "Created material: {}", path);
		mSelected = path;
		mEditor.Select(nullptr);
		BeginRename({ GetFileName(path), path, MaterialAsset::kExtension, false });
	}

	// 今のフォルダにサーフェスシェーダーの雛形 "New Shader.hlsl" を作る（Unity の Create > Shader）
	void ProjectPanel::CreateShader()
	{
		std::string path = std::format("{}/New Shader.hlsl", mFolder);
		for (int n = 1; fs::exists(ToWide(path)); ++n) { path = std::format("{}/New Shader {}.hlsl", mFolder, n); }
		if (!ShaderAsset::CreateFile(path)) { return; }
		Logger::Info(LogCat::Editor, "Created shader: {}", path);
		mSelected = path;
		mEditor.Select(nullptr);
		BeginRename({ GetFileName(path), path, ".hlsl", false });
	}

	void ProjectPanel::CreateFolder()
	{
		std::string path = mFolder + "/New Folder";
		for (int n = 1; fs::exists(ToWide(path)); ++n) { path = std::format("{}/New Folder {}", mFolder, n); }
		std::error_code ec;
		fs::create_directories(ToWide(path), ec);
		if (!ec) { BeginRename({ GetFileName(path), path, "", true }); }
	}

	// ファイルは拡張子を除いた名前だけを編集する（Unity と同じ。拡張子を消してしまう間違いを防ぐ）
	void ProjectPanel::BeginRename(const Entry& e)
	{
		mRenamePath = e.path;
		mRenameExt = e.directory ? std::string() : GetExtension(e.name);
		mRenameBuffer = e.name.substr(0, e.name.size() - mRenameExt.size());
		mOpenRename = true;
	}

	// 名前の変更と削除の確認（削除はごみ箱へ移すので元に戻せる）
	void ProjectPanel::DrawDialogs()
	{
		if (mOpenRename) { ImGui::OpenPopup(TrId("Rename##project")); mOpenRename = false; }
		ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		if (ImGui::BeginPopupModal(TrId("Rename##project"), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			if (ImGui::IsWindowAppearing()) { ImGui::SetKeyboardFocusHere(); }
			ImGui::SetNextItemWidth(ImGui::GetFontSize() * 16.0f);
			bool enter = ImGui::InputText("##name", &mRenameBuffer, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
			if (enter || ImGui::Button(Tr("Rename"), ImVec2(100.0f, 0.0f)))
			{
				std::string target = GetDirectory(mRenamePath) + mRenameBuffer + mRenameExt;
				std::error_code ec;
				if (!mRenameBuffer.empty() && target != mRenamePath && !fs::exists(ToWide(target), ec))
				{
					fs::rename(ToWide(mRenamePath), ToWide(target), ec);
					if (ec) { Logger::Error(LogCat::Editor, "Failed to rename {}: {}", mRenamePath, ec.message()); }
					else if (mSelected == mRenamePath) { mSelected = target; }
				}
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button(Tr("Cancel"), ImVec2(100.0f, 0.0f)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) { ImGui::CloseCurrentPopup(); }
			ImGui::EndPopup();
		}

		if (mOpenDelete) { ImGui::OpenPopup(TrId("Delete##project")); mOpenDelete = false; }
		const char* const kButtons[] = { Tr("Delete"), Tr("Cancel") };
		std::string message = EditorLocale::Format("Move \"{}\" to the Recycle Bin?", mDeletePath);
		int choice = -1;
		if (EditorGui::ConfirmPopup(TrId("Delete##project"), message.c_str(), kButtons, 2, choice) && choice == 0)
		{
			if (FileDialog::MoveToRecycleBin(mDeletePath))
			{
				Logger::Info(LogCat::Editor, "Moved to the Recycle Bin: {}", mDeletePath);
				if (IsInside(mSelected, mDeletePath)) { mSelected.clear(); }
				if (IsInside(mFolder, mDeletePath)) { mFolder = GetDirectory(mDeletePath); mFolder.pop_back(); }
			}
			else { Logger::Error(LogCat::Editor, "Failed to delete {}", mDeletePath); }
		}
	}
}
