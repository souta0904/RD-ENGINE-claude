#include "RdPch.h"
#include "Platform/Network/UdpSocket.h"
#include <WinSock2.h>
#include <WS2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

// mstcpip.h では SDK の版によって出ていないので自分で決める（値は Windows の決まりと同じ）
#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif

namespace rd
{
	namespace
	{
		// Winsock はソケットを使う間だけ初期化しておく（開いているソケットの数で数える）
		std::mutex sWsaMutex;
		int sWsaUsers = 0;

		bool AcquireWinsock()
		{
			std::lock_guard lock(sWsaMutex);
			if (sWsaUsers == 0)
			{
				WSADATA data;
				if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
				{
					Logger::Error(LogCat::Platform, "WSAStartup failed");
					return false;
				}
			}
			++sWsaUsers;
			return true;
		}

		void ReleaseWinsock()
		{
			std::lock_guard lock(sWsaMutex);
			if (--sWsaUsers == 0) { WSACleanup(); }
		}

		sockaddr_in ToSockAddr(const NetAddress& a)
		{
			sockaddr_in s{};
			s.sin_family = AF_INET;
			s.sin_addr.s_addr = htonl(a.ip);
			s.sin_port = htons(a.port);
			return s;
		}
	}

	// ---- NetAddress ----

	NetAddress NetAddress::Loopback(uint16_t port)
	{
		return { 0x7F000001u, port };
	}

	NetAddress NetAddress::Broadcast(uint16_t port)
	{
		return { 0xFFFFFFFFu, port };
	}

	bool NetAddress::Parse(const std::string& text, uint16_t defaultPort, NetAddress& out)
	{
		std::string host = text;
		uint16_t port = defaultPort;
		size_t colon = text.rfind(':');
		if (colon != std::string::npos)
		{
			host = text.substr(0, colon);
			int p = std::atoi(text.c_str() + colon + 1);
			if (p <= 0 || p > 65535) { return false; }
			port = uint16_t(p);
		}
		if (host.empty()) { return false; }

		// 数字の住所ならそのまま、名前なら DNS（または LAN の名前解決）で調べる
		in_addr addr{};
		if (inet_pton(AF_INET, host.c_str(), &addr) == 1)
		{
			out = { ntohl(addr.s_addr), port };
			return true;
		}
		if (!AcquireWinsock()) { return false; }
		addrinfo hints{};
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_DGRAM;
		addrinfo* result = nullptr;
		bool ok = getaddrinfo(host.c_str(), nullptr, &hints, &result) == 0 && result;
		if (ok)
		{
			auto* s = reinterpret_cast<sockaddr_in*>(result->ai_addr);
			out = { ntohl(s->sin_addr.s_addr), port };
		}
		if (result) { freeaddrinfo(result); }
		ReleaseWinsock();
		return ok;
	}

	std::string NetAddress::ToString() const
	{
		return std::format("{}.{}.{}.{}:{}", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF, port);
	}

	// ---- UdpSocket ----

	bool UdpSocket::Open(uint16_t port, bool broadcast)
	{
		Close();
		if (!AcquireWinsock()) { return false; }
		SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (s == INVALID_SOCKET)
		{
			ReleaseWinsock();
			return false;
		}
		mSocket = uintptr_t(s);

		sockaddr_in local = ToSockAddr({ 0, port }); // INADDR_ANY: どのネットワークから来ても受け取る
		if (bind(s, reinterpret_cast<sockaddr*>(&local), sizeof(local)) == SOCKET_ERROR)
		{
			Logger::Error(LogCat::Platform, "UDP port {} is not available (error {})", port, WSAGetLastError());
			Close();
			return false;
		}
		u_long nonBlocking = 1;
		ioctlsocket(s, FIONBIO, &nonBlocking);
		if (broadcast)
		{
			BOOL on = TRUE;
			setsockopt(s, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&on), sizeof(on));
		}
		// 相手がまだいないポートへ送ると、Windows は次の受信を「接続がリセットされた」で失敗させる。
		//  UDP では相手がいないのはふつうのことなので、その知らせを止める
		BOOL reportReset = FALSE;
		DWORD bytes = 0;
		WSAIoctl(s, SIO_UDP_CONNRESET, &reportReset, sizeof(reportReset), nullptr, 0, &bytes, nullptr, nullptr);
		return true;
	}

	void UdpSocket::Close()
	{
		if (mSocket == kInvalid) { return; }
		closesocket(SOCKET(mSocket));
		mSocket = kInvalid;
		ReleaseWinsock();
	}

	uint16_t UdpSocket::GetLocalPort() const
	{
		if (!IsOpen()) { return 0; }
		sockaddr_in local{};
		int len = sizeof(local);
		if (getsockname(SOCKET(mSocket), reinterpret_cast<sockaddr*>(&local), &len) == SOCKET_ERROR) { return 0; }
		return ntohs(local.sin_port);
	}

	bool UdpSocket::Send(const NetAddress& to, const void* data, size_t size)
	{
		if (!IsOpen() || size > kMaxPacketSize) { return false; }
		sockaddr_in dst = ToSockAddr(to);
		int sent = sendto(SOCKET(mSocket), static_cast<const char*>(data), int(size), 0, reinterpret_cast<sockaddr*>(&dst), sizeof(dst));
		return sent == int(size);
	}

	int UdpSocket::Receive(NetAddress& from, void* buffer, size_t capacity)
	{
		if (!IsOpen()) { return -1; }
		sockaddr_in src{};
		int len = sizeof(src);
		int n = recvfrom(SOCKET(mSocket), static_cast<char*>(buffer), int(capacity), 0, reinterpret_cast<sockaddr*>(&src), &len);
		if (n == SOCKET_ERROR)
		{
			int err = WSAGetLastError();
			// WSAEMSGSIZE: 入れ物より大きいもの（よそのプログラムが送ってきたもの）。捨てて次へ
			return err == WSAEWOULDBLOCK || err == WSAECONNRESET ? 0 : -1;
		}
		from = { ntohl(src.sin_addr.s_addr), ntohs(src.sin_port) };
		return n;
	}
}
