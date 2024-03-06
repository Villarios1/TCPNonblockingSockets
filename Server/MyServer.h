#pragma once
#include "PNet/PNet.h" //Linux требует указать папку

using namespace PNet;

class MyServer : public Server
{
	uint32_t m_latestVersionMajor = 1;
	uint32_t m_latestVersionMinor = 0;

	void onConnect(TCPConnection& newConnection) override;
	void onDisconnect(const TCPConnection& lostConnection, const std::string_view reason) override;
	bool processPacket(TCPConnection& connection, std::shared_ptr<Packet> packet) override;
};