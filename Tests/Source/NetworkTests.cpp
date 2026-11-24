#include "TestFramework.h"
#include "Platform/Network/NetPacket.h"
#include "Platform/Network/UdpSocket.h"
#include <chrono>
#include <thread>

using namespace rd;

RD_TEST(Network, PacketRoundTrip)
{
	NetWriter w;
	w.Write(uint32_t(0x12345678));
	w.WriteString("MouseNetworkTest");
	w.Write(Vector3{ 1.0f, -2.0f, 3.5f });

	NetReader r(w.Data(), w.Size());
	RD_CHECK_EQ(r.Read<uint32_t>(), 0x12345678u);
	std::string s;
	RD_CHECK(r.ReadString(s));
	RD_CHECK(s == "MouseNetworkTest");
	Vector3 v = r.Read<Vector3>();
	RD_CHECK(v.x == 1.0f && v.y == -2.0f && v.z == 3.5f);
	RD_CHECK(r.IsOk());
	RD_CHECK_EQ(r.Remaining(), size_t(0));

	// 足りない所を読もうとしたら失敗し、その後もずっと失敗のまま
	r.Read<uint8_t>();
	RD_CHECK(!r.IsOk());
	NetReader broken(w.Data(), 6);
	std::string cut;
	broken.Read<uint32_t>();
	RD_CHECK(!broken.ReadString(cut));
}

RD_TEST(Network, ParseAddress)
{
	NetAddress a;
	RD_CHECK(NetAddress::Parse("192.168.0.3", 27015, a));
	RD_CHECK_EQ(a.ip, 0xC0A80003u);
	RD_CHECK_EQ(a.port, uint16_t(27015));
	RD_CHECK(NetAddress::Parse("127.0.0.1:7777", 27015, a));
	RD_CHECK(a == NetAddress::Loopback(7777));
	RD_CHECK(a.ToString() == "127.0.0.1:7777");
	RD_CHECK(!NetAddress::Parse("127.0.0.1:0", 27015, a));
	RD_CHECK(!NetAddress::Parse("", 27015, a));
}

RD_TEST(Network, UdpLoopback)
{
	UdpSocket receiver;
	RD_CHECK(receiver.Open(0));
	uint16_t port = receiver.GetLocalPort();
	RD_CHECK(port != 0);

	// 同じポートは 2 つ目では開けない（ホストが 2 つ立たないようにするため）
	UdpSocket same;
	RD_CHECK(!same.Open(port));

	UdpSocket sender;
	RD_CHECK(sender.Open(0, true));
	// 届いていないうちは待たずに 0 が返る
	uint8_t buffer[UdpSocket::kMaxPacketSize];
	NetAddress from;
	RD_CHECK_EQ(receiver.Receive(from, buffer, sizeof(buffer)), 0);

	const char message[] = "hello";
	RD_CHECK(sender.Send(NetAddress::Loopback(port), message, sizeof(message)));
	int n = 0;
	for (int i = 0; i < 100 && n == 0; ++i)
	{
		n = receiver.Receive(from, buffer, sizeof(buffer));
		if (n == 0) { std::this_thread::sleep_for(std::chrono::milliseconds(5)); }
	}
	RD_CHECK_EQ(n, int(sizeof(message)));
	RD_CHECK(std::memcmp(buffer, message, sizeof(message)) == 0);
	RD_CHECK_EQ(from.port, sender.GetLocalPort());

	// 誰も待っていないポートへ送っても、次の受信は失敗にならない（Windows の UDP の知らせを止めている）
	UdpSocket lonely;
	RD_CHECK(lonely.Open(0));
	uint16_t closedPort = receiver.GetLocalPort();
	receiver.Close();
	lonely.Send(NetAddress::Loopback(closedPort), message, sizeof(message));
	std::this_thread::sleep_for(std::chrono::milliseconds(20));
	RD_CHECK_EQ(lonely.Receive(from, buffer, sizeof(buffer)), 0);
}
