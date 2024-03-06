#pragma once
#ifdef _WIN32
#include "PNet.h"
#else
#include "PNet/PNet.h" //Linux требует указать папку
#endif

using namespace PNet;

class MyServer : public Server
{
	uint32_t m_latestVersionMajor = 1;
	uint32_t m_latestVersionMinor = 0;

	void onConnect(TCPConnection& newConnection) override;
	void onDisconnect(const TCPConnection& lostConnection, const std::string_view reason) override;
	bool processPacket(TCPConnection& connection, std::shared_ptr<Packet> packet) override;
};