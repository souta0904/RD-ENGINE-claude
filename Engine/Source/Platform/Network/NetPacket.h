#pragma once
#include "Math/Quaternion.h"
#include "Math/Vector3.h"
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

namespace rd
{
	// 送る中身を詰める（数はリトルエンディアンのまま並べる。送る側も受ける側も Windows の x64 なので並べ替えない）
	class NetWriter
	{
	public:
		template <class T> requires std::is_trivially_copyable_v<T>
		void Write(const T& value)
		{
			size_t at = mData.size();
			mData.resize(at + sizeof(T));
			std::memcpy(mData.data() + at, &value, sizeof(T));
		}
		void WriteString(const std::string& s)
		{
			uint16_t len = uint16_t(std::min<size_t>(s.size(), 0xFFFF));
			Write(len);
			mData.insert(mData.end(), s.begin(), s.begin() + len);
		}
		void WriteBytes(const void* data, size_t size)
		{
			auto* p = static_cast<const uint8_t*>(data);
			mData.insert(mData.end(), p, p + size);
		}

		const uint8_t* Data() const { return mData.data(); }
		size_t Size() const { return mData.size(); }
		void Clear() { mData.clear(); }

	private:
		std::vector<uint8_t> mData;
	};

	// 受け取った中身を順に取り出す。足りないところを読もうとしたら以降は失敗のまま（IsOk が false）
	class NetReader
	{
	public:
		NetReader(const void* data, size_t size) : mData(static_cast<const uint8_t*>(data)), mSize(size) {}

		template <class T> requires std::is_trivially_copyable_v<T>
		bool Read(T& out)
		{
			if (!mOk || mPos + sizeof(T) > mSize) { mOk = false; return false; }
			std::memcpy(&out, mData + mPos, sizeof(T));
			mPos += sizeof(T);
			return true;
		}
		template <class T> requires std::is_trivially_copyable_v<T>
		T Read()
		{
			T value{};
			Read(value);
			return value;
		}
		bool ReadString(std::string& out)
		{
			uint16_t len = 0;
			if (!Read(len) || mPos + len > mSize) { mOk = false; return false; }
			out.assign(reinterpret_cast<const char*>(mData + mPos), len);
			mPos += len;
			return true;
		}
		// size だけ読み飛ばした所を返す（足りなければ nullptr）
		const uint8_t* ReadBytes(size_t size)
		{
			if (!mOk || mPos + size > mSize) { mOk = false; return nullptr; }
			const uint8_t* p = mData + mPos;
			mPos += size;
			return p;
		}

		bool IsOk() const { return mOk; }
		size_t Remaining() const { return mOk ? mSize - mPos : 0; }

	private:
		const uint8_t* mData;
		size_t mSize;
		size_t mPos = 0;
		bool mOk = true;
	};
}
