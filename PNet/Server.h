#pragma once
#include "TCPConnection.h"
#include <iostream>

namespace PNet
{
	class Server
	{
	public:
		bool initialize(std::unique_ptr<IPEndpoint> ipEndpoint);
		void frame();
	protected:
		virtual void onConnect(TCPConnection& newConnection);
		virtual void onDisconnect(const TCPConnection& lostConnection, const std::string_view reason);
		virtual bool processPacket(TCPConnection& connection, std::shared_ptr<Packet> packet);

		void closeConnection(const int connectionIndex, const std::string_view reason);
		std::vector<TCPConnection> m_connections;
		std::vector<pollfd> m_master_fd;
	private:
		Socket m_listeningSocket;
	};
}