#pragma once
#include "Socket.h"
#include "PacketManager.h"

#ifndef _WIN32
#include <sys/poll.h> //pollfd Linux в Client и Server
#define WSAPoll poll
#define WSAEWOULDBLOCK EWOULDBLOCK
#endif

namespace PNet
{
	class TCPConnection
	{
		std::string m_connectionInfoForServer = "";
		std::unique_ptr<IPEndpoint> m_ipEndpoint;
		Socket m_socket;
	public:
		std::unique_ptr<PacketManager> m_pmIncoming = std::make_unique<PacketManager>();
		std::unique_ptr<PacketManager> m_pmOutgoing = std::make_unique<PacketManager>();
		char m_buffer[PNet::g_MaxPacketSize]; //�.� �� �� ��������� �����, �� �� �� ����� ��������� ���� ����� �������������� ������

	public:
		TCPConnection(Socket& socket, std::unique_ptr<IPEndpoint> ipEndpoint);
		TCPConnection() = default; //��� �������� ������� ������� �� ���������

		std::string_view getConnectionInfo() const;
		void closeTCPConnection();
	};
}