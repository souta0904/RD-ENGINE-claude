#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace rd
{
	// 通信の相手の住所（IPv4 のアドレスとポート。どちらもふつうの数の並び＝ホストのバイト順で持つ）
	struct RD_API NetAddress
	{
		uint32_t ip = 0;
		uint16_t port = 0;

		// 同じ PC（127.0.0.1）
		static NetAddress Loopback(uint16_t port);
		// 同じ LAN の全員（255.255.255.255。UdpSocket を broadcast 付きで開いたときだけ送れる）
		static NetAddress Broadcast(uint16_t port);
		// "192.168.0.3" / "192.168.0.3:7777" / "my-pc" のような文字から作る（ポートがなければ defaultPort）
		static bool Parse(const std::string& text, uint16_t defaultPort, NetAddress& out);

		bool IsValid() const { return port != 0; }
		std::string ToString() const;
		bool operator==(const NetAddress& o) const = default;
	};

	// UDP のソケット（Winsock）。待たずに使う: Receive は届いているものがなければすぐ 0 を返す
	//  届く順番も、届くかどうかも保証されないので、使う側で何度も送り直すなどして扱う
	class RD_API UdpSocket
	{
	public:
		static constexpr size_t kMaxPacketSize = 1200;

		UdpSocket() = default;
		~UdpSocket() { Close(); }
		UdpSocket(const UdpSocket&) = delete;
		UdpSocket& operator=(const UdpSocket&) = delete;

		// port 0 なら空いているポートを OS が選ぶ。broadcast なら LAN 全体へ送れるようにする
		bool Open(uint16_t port = 0, bool broadcast = false);
		void Close();
		bool IsOpen() const { return mSocket != kInvalid; }
		uint16_t GetLocalPort() const;

		bool Send(const NetAddress& to, const void* data, size_t size);
		// 届いていれば 1 つ受け取ってその大きさを返す（なければ 0、壊れていたら -1）
		int Receive(NetAddress& from, void* buffer, size_t capacity);

	private:
		static constexpr uintptr_t kInvalid = ~uintptr_t(0);
		uintptr_t mSocket = kInvalid;
	};
}
