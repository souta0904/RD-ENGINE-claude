#include "RdPch.h"
#include "Editor/Core/AssetImporter.h"
#include "Asset/Model/Model.h"
#include "Asset/Model/ModelData.h"
#include "Asset/Model/ModelFile.h"
#include "Asset/Model/ModelImporter.h"
#include "Core/Jobs/JobSystem.h"
#include "Core/Util/StringUtil.h"
#include <DirectXTex.h>
#include <fstream>

namespace rd
{
	namespace
	{
		namespace fs = std::filesystem;

		// 取り込むファイル 1 つと、その置き先のフォルダ
		struct ImportItem
		{
			fs::path source;
			fs::path folder;
		};

		// コピー元のパス → コピー先のパス（同じテクスチャを 2 回コピーしないために覚えておく）
		using CopiedFiles = std::unordered_map<std::wstring, fs::path>;

		std::string ExtensionOf(const fs::path& path) { return ToLower(ToUtf8(path.extension().wstring())); }
		std::string ToText(const fs::path& path) { return ToUtf8(path.generic_wstring()); }

		std::wstring CanonicalKey(const fs::path& path)
		{
			std::error_code ec;
			fs::path canonical = fs::weakly_canonical(path, ec);
			return (ec ? path : canonical).native();
		}

		// child が folder の中（folder 自身を含む）にあるか
		bool IsInsideFolder(const fs::path& child, const fs::path& folder)
		{
			std::error_code ec;
			fs::path a = fs::weakly_canonical(child, ec);
			if (ec) { return false; }
			fs::path b = fs::weakly_canonical(folder, ec);
			if (ec) { return false; }
			fs::path relative = a.lexically_relative(b);
			return !relative.empty() && *relative.begin() != L"..";
		}

		// "name.ext"、"name 1.ext"、"name 2.ext" … の順に候補を作り、accept が true を返した最初のものを返す
		template <class Accept>
		fs::path FindName(const fs::path& folder, const std::wstring& stem, const std::wstring& ext, Accept accept)
		{
			for (int n = 0;; ++n)
			{
				fs::path candidate = folder / (n == 0 ? stem + ext : std::format(L"{} {}{}", stem, n, ext));
				if (accept(candidate)) { return candidate; }
			}
		}

		fs::path UniquePath(const fs::path& folder, const std::wstring& stem, const std::wstring& ext)
		{
			return FindName(folder, stem, ext, [](const fs::path& p) { std::error_code ec; return !fs::exists(p, ec); });
		}

		bool HasSameContent(const fs::path& a, const fs::path& b)
		{
			std::error_code ec;
			auto size = fs::file_size(a, ec);
			if (ec || size != fs::file_size(b, ec) || ec) { return false; }
			std::ifstream fa(a, std::ios::binary), fb(b, std::ios::binary);
			std::vector<char> ba(1 << 16), bb(1 << 16);
			while (fa && fb)
			{
				fa.read(ba.data(), std::streamsize(ba.size()));
				fb.read(bb.data(), std::streamsize(bb.size()));
				if (fa.gcount() != fb.gcount() || std::memcmp(ba.data(), bb.data(), size_t(fa.gcount())) != 0) { return false; }
			}
			return true;
		}

		// 同じ中身のファイルがあればそれを使い、なければ空いている名前でコピーする。失敗したら空を返す
		fs::path CopyIntoFolder(const fs::path& source, const fs::path& folder)
		{
			fs::path target = FindName(folder, source.stem().wstring(), source.extension().wstring(), [&](const fs::path& p)
				{
					std::error_code ec;
					return !fs::exists(p, ec) || HasSameContent(source, p);
				});
			std::error_code ec;
			if (fs::exists(target, ec)) { return target; }
			fs::copy_file(source, target, ec);
			if (ec)
			{
				Log::Error(LogCat::Editor, "Failed to copy {}: {}", ToText(source), ec.message());
				return {};
			}
			return target;
		}

		// 埋め込みのテクスチャをファイルにする（圧縮された画像はそのまま、画素の並びは TGA で保存する）
		fs::path WriteEmbeddedTexture(const EmbeddedTexture& texture, const fs::path& folder, const std::wstring& modelStem, size_t index)
		{
			std::wstring stem = texture.name.empty() || texture.name[0] == '*'
				? std::format(L"{}_texture{}", modelStem, index)
				: ToWide(texture.name);

			if (texture.height > 0)
			{
				fs::path target = UniquePath(folder, stem, L".tga");
				std::vector<uint8_t> pixels = texture.data;
				DirectX::Image image = {};
				image.width = texture.width;
				image.height = texture.height;
				image.format = DXGI_FORMAT_R8G8B8A8_UNORM;
				image.rowPitch = size_t(texture.width) * 4;
				image.slicePitch = image.rowPitch * texture.height;
				image.pixels = pixels.data();
				if (FAILED(DirectX::SaveToTGAFile(image, DirectX::TGA_FLAGS_NONE, target.c_str())))
				{
					Log::Error(LogCat::Editor, "Failed to write texture {}", ToText(target));
					return {};
				}
				return target;
			}

			std::wstring ext = L"." + ToWide(texture.format.empty() ? "png" : texture.format);
			auto sameBytes = [&](const fs::path& p)
				{
					std::error_code ec;
					if (fs::file_size(p, ec) != texture.data.size() || ec) { return false; }
					std::ifstream file(p, std::ios::binary);
					std::vector<char> bytes(texture.data.size());
					file.read(bytes.data(), std::streamsize(bytes.size()));
					return file.good() && std::memcmp(bytes.data(), texture.data.data(), bytes.size()) == 0;
				};
			fs::path target = FindName(folder, stem, ext, [&](const fs::path& p)
				{
					std::error_code ec;
					return !fs::exists(p, ec) || sameBytes(p);
				});
			std::error_code ec;
			if (fs::exists(target, ec)) { return target; }
			std::ofstream file(target, std::ios::binary);
			file.write(reinterpret_cast<const char*>(texture.data.data()), std::streamsize(texture.data.size()));
			if (!file.good())
			{
				Log::Error(LogCat::Editor, "Failed to write texture {}", ToText(target));
				return {};
			}
			return target;
		}

		// マテリアルのテクスチャを取り込み先へそろえ、.rdmodel からの相対パスに書き換える
		void PrepareTextures(ModelData& data, const fs::path& source, const fs::path& folder, CopiedFiles& copied)
		{
			std::vector<fs::path> embedded(data.embeddedTextures.size());
			auto convert = [&](std::string& path)
				{
					if (path.empty()) { return; }
					std::string name = path;
					if (EndsWith(name, "#linear")) { name.resize(name.size() - std::strlen("#linear")); }

					fs::path file;
					if (name.starts_with(ModelData::kEmbeddedPrefix))
					{
						size_t index = std::strtoull(name.c_str() + std::strlen(ModelData::kEmbeddedPrefix), nullptr, 10);
						if (embedded[index].empty())
						{
							embedded[index] = WriteEmbeddedTexture(data.embeddedTextures[index], folder, source.stem().wstring(), index);
						}
						file = embedded[index];
					}
					else
					{
						fs::path original = ToWide(name);
						std::error_code ec;
						std::wstring key = CanonicalKey(original);
						if (auto it = copied.find(key); it != copied.end()) { file = it->second; }
						else if (!fs::exists(original, ec)) { Log::Warning(LogCat::Editor, "Texture not found: {}", name); }
						else if (IsInsideFolder(original, L"Assets")) { file = original; }
						else
						{
							file = CopyIntoFolder(original, folder);
							if (!file.empty()) { copied.emplace(std::move(key), file); }
						}
					}

					path.clear();
					if (file.empty()) { return; }
					std::error_code ec;
					fs::path relative = fs::relative(file, folder, ec);
					if (!ec && !relative.empty()) { path = ToText(relative); }
				};
			for (auto& material : data.materials)
			{
				convert(material.albedoPath);
				convert(material.normalPath);
			}
		}

		std::string FormatSize(uintmax_t bytes)
		{
			return std::format("{:.2f} MB", double(bytes) / (1024.0 * 1024.0));
		}

		bool ConvertModel(const fs::path& source, const fs::path& folder, CopiedFiles& copied)
		{
			ModelData data;
			std::string error;
			if (!ModelImporter::Import(ToText(source), false, data, error))
			{
				Log::Error(LogCat::Editor, "Failed to import {}: {}", ToText(source), error);
				return false;
			}
			PrepareTextures(data, source, folder, copied);

			fs::path target = UniquePath(folder, source.stem().wstring(), ToWide(ModelFile::kExtension));
			if (!ModelFile::Write(ToText(target), data, error))
			{
				Log::Error(LogCat::Editor, "Failed to write {}: {}", ToText(target), error);
				return false;
			}
			std::error_code ec;
			auto before = fs::file_size(source, ec);
			auto after = fs::file_size(target, ec);
			Log::Info(LogCat::Editor, "Converted {} -> {} ({} -> {})", ToText(source.filename()), ToText(target), FormatSize(before), FormatSize(after));
			return true;
		}

		// フォルダを開いて、取り込むファイルの一覧を作る（取り込み先のフォルダもここで作る）
		void CollectItems(const fs::path& source, const fs::path& folder, std::vector<ImportItem>& items)
		{
			std::error_code ec;
			if (!fs::is_directory(source, ec))
			{
				items.push_back({ source, folder });
				return;
			}
			if (IsInsideFolder(folder, source))
			{
				Log::Warning(LogCat::Editor, "Cannot import {} into itself", ToText(source));
				return;
			}
			std::vector<fs::path> children;
			for (auto& entry : fs::directory_iterator(source, ec)) { children.push_back(entry.path()); }
			fs::path target = UniquePath(folder, source.filename().wstring(), L"");
			fs::create_directories(target, ec);
			if (ec)
			{
				Log::Error(LogCat::Editor, "Failed to create {}: {}", ToText(target), ec.message());
				return;
			}
			for (const auto& child : children) { CollectItems(child, target, items); }
		}
	}

	uint32_t AssetImporter::Import(const std::vector<fs::path>& sources, const std::string& folder)
	{
		fs::path destination = ToWide(folder);
		std::vector<ImportItem> items;
		for (const auto& source : sources) { CollectItems(source, destination, items); }

		// モデルのあるフォルダの .mtl / .bin はモデルの一部で、変換後はいらない
		std::unordered_set<std::wstring> modelFolders;
		for (const auto& item : items)
		{
			if (Model::IsSourceExtension(ExtensionOf(item.source))) { modelFolders.insert(item.source.parent_path().native()); }
		}

		// モデルが同じテクスチャをもう一度コピーしないよう、先にモデル以外をコピーする
		uint32_t imported = 0;
		CopiedFiles copied;
		for (const auto& item : items)
		{
			std::string ext = ExtensionOf(item.source);
			if (Model::IsSourceExtension(ext)) { continue; }
			if ((ext == ".mtl" || ext == ".bin") && modelFolders.contains(item.source.parent_path().native())) { continue; }
			fs::path target = CopyIntoFolder(item.source, item.folder);
			if (target.empty()) { continue; }
			copied.emplace(CanonicalKey(item.source), target);
			Log::Info(LogCat::Editor, "Imported {}", ToText(target));
			++imported;
		}
		for (const auto& item : items)
		{
			if (Model::IsSourceExtension(ExtensionOf(item.source)) && ConvertModel(item.source, item.folder, copied)) { ++imported; }
		}
		return imported;
	}

	void AssetImporter::ImportAsync(std::vector<fs::path> sources, std::string folder)
	{
		++sPending;
		JobSystem::Schedule([sources = std::move(sources), folder = std::move(folder)]
			{
				try { Import(sources, folder); }
				catch (const std::exception& e) { Log::Error(LogCat::Editor, "Import failed: {}", e.what()); }
				--sPending;
			}, JobPriority::Low, "Asset Import");
	}
}
