#pragma once
#include "RHI/Shader/Shader.h"
#include <memory>
#include <mutex>
#include <unordered_map>

namespace rd
{
	// DXC のシェーダーコンパイラ（結果はキャッシュする。ClearCache でホットリロード）
	class RD_API ShaderCompiler
	{
	public:
		ShaderCompiler() = default;
		// RD_API で DLL から出すと、コピーできないメンバーのコピーまで作ろうとするので禁止しておく
		ShaderCompiler(const ShaderCompiler&) = delete;
		ShaderCompiler& operator=(const ShaderCompiler&) = delete;
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
