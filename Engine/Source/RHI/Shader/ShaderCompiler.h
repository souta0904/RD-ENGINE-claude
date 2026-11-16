#pragma once
#include "RHI/Shader/Shader.h"
#include <memory>
#include <mutex>
#include <unordered_map>

namespace rd
{
	// DXC のシェーダーコンパイラ（結果はキャッシュする。ClearCache でホットリロード）
	class ShaderCompiler
	{
	public:
		void Initialize();
		const Shader* Compile(const ShaderDesc& desc);
		void ClearCache();

	private:
		ComPtr<IDxcUtils> mUtils;
		ComPtr<IDxcCompiler3> mCompiler;
		ComPtr<IDxcIncludeHandler> mIncludeHandler;
		std::unordered_map<std::string, std::unique_ptr<Shader>> mCache;
		std::mutex mMutex;
	};
}
