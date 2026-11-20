#pragma once
#include "Core/Serialization/Json.h"
#include "Scene/Component/Component.h"
#include <string>

namespace rd
{
	// 型が見つからないコンポーネントの代わり（Unity の「Missing Script」にあたる）
	//  シーンやプレハブを読んだときに、保存されていた型がもう登録されていなければ、これを置く。
	//  保存されていた値（JSON）をそのまま持ち、保存するときは元の型名と値で書き戻すので、データは消えない。
	//  ゲームのコードを読み直してクラスが戻れば、シーンを作り直したときに元のコンポーネントに戻る。
	//  クラスの名前を変えたときなどに、うっかり値を失わないための仕組み
	class RD_API MissingComponent final : public Component
	{
	public:
		static constexpr const char* kTypeName = "MissingComponent";

		// data: 保存されていたコンポーネントの JSON（"Type" と "Enabled" も含む）
		MissingComponent(std::string typeName, Json data);

		// 保存するときの型名は、元の型の名前にする
		const char* GetTypeName() const override { return mTypeName.c_str(); }
		const ComponentTypeInfo& GetTypeInfo() const override { return GetComponentTypeInfo<MissingComponent>(); }
		template <class> friend constexpr uint32_t (detail::ComputeCallbacks)();

		const Json& GetData() const { return mData; }
		// Undo などで値を書き戻すとき
		void SetData(Json data) { mData = std::move(data); }

	private:
		std::string mTypeName;
		Json mData;
	};
}
