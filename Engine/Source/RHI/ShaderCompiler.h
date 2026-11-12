#pragma once
#include "RHI/D3D12Common.h"
#include <dxcapi.h>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace rd
{
	enum class ShaderStage : uint8_t { Vertex, Pixel, Compute, Geometry };

	struct ShaderDesc
	{
		std::string path;
		std::string entry = "main";
		ShaderStage stage = ShaderStage::Vertex;
		std::vector<std::string> defines;// "NAME" または "NAME=VALUE"

		std::string GetKey() const;
	};

	// コンパイル済みシェーダ
	struct Shader
	{
		ComPtr<IDxcBlob> blob;
		D3D12_SHADER_BYTECODE GetBytecode() const
		{
			return blob ? D3D12_SHADER_BYTECODE{ blob->GetBufferPointer(), blob->GetBufferSize() } : D3D12_SHADER_BYTECODE{};
		}
		bool IsValid() const { return blob != nullptr; }
	};

	// DXC によるシェーダコンパイラ（結果はキャッシュされる）
	class ShaderCompiler
	{
	public:
		void Initialize();
		const Shader* Compile(const ShaderDesc& desc);
		// キャッシュを捨てて再コンパイルさせる（ホットリロード用）
		void ClearCache();

	private:
		ComPtr<IDxcUtils> mUtils;
		ComPtr<IDxcCompiler3> mCompiler;
		ComPtr<IDxcIncludeHandler> mIncludeHandler;
		std::unordered_map<std::string, std::unique_ptr<Shader>> mCache;
		std::mutex mMutex;
	};
}
