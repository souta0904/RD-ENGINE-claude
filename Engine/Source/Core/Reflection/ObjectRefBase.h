#pragma once
#include <cstdint>

namespace rd
{
	// 参照プロパティの基底（中身は参照先の ID だけ）
	//  Core はシーンを知らないので ID の読み書きだけを扱う。実際に使うのは Scene の ActorRef / ComponentRef<T>
	//
	//  なぜポインタではなく ID なのか
	//    ポインタはファイルに保存できない（次に起動したときには別の場所にある）。
	//    アクターごとの番号（ID）で持てば、保存して読み直しても同じ相手を指せる。
	//    参照先が消えても、ID で探して見つからないだけなので、消えたものを触って落ちることがない
	//  0 は「何も指していない」
	class ObjectRefBase
	{
	public:
		uint64_t GetId() const { return mId; }
		void SetId(uint64_t id) { mId = id; }
		void Clear() { mId = 0; }

	protected:
		// protected … この型だけでは作れず、継承した ActorRef などからだけ作れる
		ObjectRefBase() = default;
		uint64_t mId = 0;
	};
}
