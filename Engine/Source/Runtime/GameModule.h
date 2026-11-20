#pragma once
#include <filesystem>
#include <string>

namespace rd
{
	struct EngineConfig;

	// ゲームのコードを入れた DLL（エディタ入りの構成だけで使う。Game 構成ではゲームを exe に静的リンクする）
	//
	//  エディタ（RdEditor.exe）は起動するときに -game の名前の DLL を読み、ゲームの設定の関数（RD_GAME_MAIN で
	//  作られる RdConfigureGame）を呼ぶ。DLL を読み込むと、その中の RD_REGISTER_COMPONENT が動いて
	//  コンポーネントが ComponentRegistry に登録される。
	//
	//  ビルドし直した DLL は Reload で読み直す（Unreal のホットリロードと同じ考え方）。
	//   - ビルドの出力（exe の隣の <名前>.dll）は読み込まずに、毎回 HotReload/ へコピーしたものを読む。
	//     読み込み中の DLL は上書きできないので、こうしないと次のビルドが失敗する
	//   - 古い DLL は外さずに残す。古いコードを指したもの（文字列・関数・プロファイラの区間の名前など）が
	//     どこかに残っていても落ちないようにするため。終了するときに Windows が外す
	//  シーンを作り直すのは呼ぶ側（Engine）の役目
	class RD_API GameModule
	{
	public:
		using ConfigureFunc = void (*)(EngineConfig&);

		// name.dll を読む（最初の 1 回）。失敗したら false で、理由を error に入れる
		static bool Load(const std::string& name, std::string& error);
		// ビルドし直した DLL を読み直す。失敗したら今までの DLL のまま動き続ける
		static bool Reload(std::string& error);

		static bool IsLoaded() { return sReloadCount >= 0; }
		static const std::string& GetName() { return sName; }
		// ゲームの設定の関数（Load したものだけ。読み直しても起動の設定はやり直さない）
		static ConfigureFunc GetConfigure() { return sConfigure; }
		// 読み直した回数（最初に読んだだけなら 0）
		static int GetReloadCount() { return sReloadCount; }

		// ビルドの出力の場所（exe の隣の <名前>.dll）
		static std::filesystem::path GetBuiltPath();
		// 読んだときより新しい DLL がビルドされていて、書き込みも終わっているか
		static bool HasNewerBuild();
		// ビルドの出力の更新日時（なければ既定値）
		static std::filesystem::file_time_type GetBuiltWriteTime();

	private:
		static bool LoadCopy(int module, std::string& error);

		static inline std::string sName;
		static inline ConfigureFunc sConfigure = nullptr;
		static inline int sReloadCount = -1;
		static inline std::filesystem::file_time_type sLoadedTime;
	};
}
