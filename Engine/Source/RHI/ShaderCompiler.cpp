#include "RdPch.h"
#include "RHI/ShaderCompiler.h"
#include "Core/StringUtil.h"

namespace rd
{
	namespace
	{
		const wchar_t* Profile(ShaderStage stage)
		{
			switch (stage)
			{
			case ShaderStage::Pixel: return L"ps_6_0";
			case ShaderStage::Compute: return L"cs_6_0";
			case ShaderStage::Geometry: return L"gs_6_0";
			default: return L"vs_6_0";
			}
		}
	}

	std::string ShaderDesc::GetKey() const
	{
		std::string key = path + "|" + entry + "|" + std::to_string(int(stage));
		for (const auto& d : defines) { key += "|" + d; }
		return key;
	}

	void ShaderCompiler::Initialize()
	{
		RD_CHECK_HR(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&mUtils)));
		RD_CHECK_HR(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&mCompiler)));
		RD_CHECK_HR(mUtils->CreateDefaultIncludeHandler(&mIncludeHandler));
	}

	const Shader* ShaderCompiler::Compile(const ShaderDesc& desc)
	{
		std::lock_guard lock(mMutex);
		std::string key = desc.GetKey();
		if (auto it = mCache.find(key); it != mCache.end()) { return it->second.get(); }

		auto shader = std::make_unique<Shader>();
		std::wstring wpath = ToWide(desc.path);

		ComPtr<IDxcBlobEncoding> source;
		if (FAILED(mUtils->LoadFile(wpath.c_str(), nullptr, &source)))
		{
			Log::Error("Shader not found: {}", desc.path);
			return mCache.emplace(key, std::move(shader)).first->second.get();
		}

		std::wstring wentry = ToWide(desc.entry);
		std::vector<std::wstring> wdefines;
		for (const auto& d : desc.defines) { wdefines.push_back(ToWide(d)); }

		std::vector<LPCWSTR> args = {
			wpath.c_str(),
			L"-E", wentry.c_str(),
			L"-T", Profile(desc.stage),
			L"-Zpr",// 行優先（C++ 側の Matrix4 とレイアウトを合わせる）
			L"-HV", L"2021",
#if RD_DEBUG
			L"-Zi", L"-Qembed_debug", L"-Od",
#else
			L"-O3",
#endif
		};
		for (const auto& d : wdefines)
		{
			args.push_back(L"-D");
			args.push_back(d.c_str());
		}

		DxcBuffer buffer = {};
		buffer.Ptr = source->GetBufferPointer();
		buffer.Size = source->GetBufferSize();
		buffer.Encoding = DXC_CP_UTF8;

		ComPtr<IDxcResult> result;
		HRESULT hr = mCompiler->Compile(&buffer, args.data(), UINT32(args.size()), mIncludeHandler.Get(), IID_PPV_ARGS(&result));
		if (SUCCEEDED(hr))
		{
			ComPtr<IDxcBlobUtf8> errors;
			result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
			result->GetStatus(&hr);
			if (errors && errors->GetStringLength() > 0)
			{
				if (FAILED(hr)) { Log::Error("Shader compile error ({}):\n{}", desc.path, errors->GetStringPointer()); }
				else { Log::Warning("Shader warning ({}):\n{}", desc.path, errors->GetStringPointer()); }
			}
			if (SUCCEEDED(hr))
			{
				result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shader->blob), nullptr);
			}
		}
		if (shader->blob)
		{
			Log::Info("Compiled shader: {} [{}]", desc.path, desc.entry);
		}
		return mCache.emplace(key, std::move(shader)).first->second.get();
	}

	void ShaderCompiler::ClearCache()
	{
		std::lock_guard lock(mMutex);
		mCache.clear();
	}
}
