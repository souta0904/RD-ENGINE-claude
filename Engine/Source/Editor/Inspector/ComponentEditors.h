#pragma once
#include "Scene/Component/Component.h"
#include <functional>

namespace rd
{
	// コンポーネントの型ごとに、プロパティ画面の Reflect の項目の下へ出す追加の UI（Unity の CustomEditor にあたる）
	//  ボタン（Bake・Preview など）や、値ではない情報（再生中の時間など）を出すのに使う。
	//  画面の部品（ImGui）はエディタの中だけで使う決まりなので、コンポーネントの中には書かずにここへ登録する。
	//  関数は、保存する値を変えたら true を返す（Undo に積み、シーンを「変更あり」にする）。
	//
	//  使い方（エディタのある構成だけ。ゲームのコードからも登録できる）:
	//    ComponentEditors::Register<AudioSource>([](AudioSource& s) { if (ImGui::Button("Preview")) { s.Play(); } return false; });
	class RD_API ComponentEditors
	{
	public:
		using DrawFunction = std::function<bool(Component&)>;

		// 型ごとに 1 つ。同じ型で登録し直すと置き換える（ホットリロードで登録し直しても増えない）
		template <class T>
		static void Register(std::function<bool(T&)> draw)
		{
			RegisterType(typeid(T), [draw](Component& c) { return draw(static_cast<T&>(c)); });
		}

		// component の型に登録があれば描く。保存する値を変えたら true
		//  型は実際のクラス（typeid）で探すので、MissingComponent が同じ名前を持っていても間違えない
		static bool Draw(Component& component);

		// エンジンの組み込みコンポーネントの分を登録する（BuiltinComponentEditors.cpp）
		static void RegisterBuiltin();

	private:
		static void RegisterType(std::type_index type, DrawFunction draw);
	};
}
