#pragma once
#include "RHI/D3D12Common.h"
#include <dxcapi.h>
#include <string>
#include <vector>

namespace rd
{
	enum class ShaderStage : uint8_t { Vertex, Pixel, Compute, Geometry };

	// コンパイルするシェーダーの指定（defines は "NAME" か "NAME=VALUE"）
	struct ShaderDesc
	{
		std::string path;
		std::string entry = "main";
		ShaderStage stage = ShaderStage::Vertex;
		std::vector<std::string> defines;

		std::string GetKey() const;
	};

	// コンパイル済みのシェーダー
	struct Shader
	{
		ComPtr<IDxcBlob> blob;
		D3D12_SHADER_BYTECODE GetBytecode() const
		{
			return blob ? D3D12_SHADER_BYTECODE{ blob->GetBufferPointer(), blob->GetBufferSize() } : D3D12_SHADER_BYTECODE{};
		}
		bool IsValid() const { return blob != nullptr; }
	};
}
