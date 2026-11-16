#pragma once
#include <cstdint>

namespace rd
{
	// 参照プロパティの基底（中身は参照先の ID だけ）
	//  Core はシーンを知らないので ID の読み書きだけを扱う。実際に使うのは Scene の ActorRef / ComponentRef<T>
	class ObjectRefBase
	{
	public:
		uint64_t GetId() const { return mId; }
		void SetId(uint64_t id) { mId = id; }
		void Clear() { mId = 0; }

	protected:
		ObjectRefBase() = default;
		uint64_t mId = 0;
	};
}
