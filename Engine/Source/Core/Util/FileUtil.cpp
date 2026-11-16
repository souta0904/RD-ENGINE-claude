#include "RdPch.h"
#include "Core/Util/FileUtil.h"
#include "Core/Util/StringUtil.h"
#include <fstream>
#include <sstream>

namespace rd
{
	bool ReadTextFile(const std::string& path, std::string& out)
	{
		std::ifstream file(std::filesystem::path(ToWide(path)), std::ios::binary);
		if (!file) { return false; }
		std::stringstream ss;
		ss << file.rdbuf();
		out = ss.str();
		return true;
	}

	bool ReadBinaryFile(const std::string& path, std::vector<uint8_t>& out)
	{
		std::ifstream file(std::filesystem::path(ToWide(path)), std::ios::binary | std::ios::ate);
		if (!file) { return false; }
		auto size = file.tellg();
		out.resize(size_t(size));
		file.seekg(0);
		file.read(reinterpret_cast<char*>(out.data()), size);
		return bool(file);
	}

	bool WriteTextFile(const std::string& path, std::string_view text)
	{
		std::filesystem::path fsPath(ToWide(path));
		if (fsPath.has_parent_path())
		{
			std::error_code ec;
			std::filesystem::create_directories(fsPath.parent_path(), ec);
		}
		std::ofstream file(fsPath, std::ios::binary);
		if (!file) { return false; }
		file.write(text.data(), std::streamsize(text.size()));
		return bool(file);
	}

	bool FileExists(const std::string& path)
	{
		std::error_code ec;
		return std::filesystem::exists(std::filesystem::path(ToWide(path)), ec);
	}
}
