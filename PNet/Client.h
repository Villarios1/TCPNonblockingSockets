#pragma once
#include "TCPConnection.h"
#include <iostream>

namespace PNet
{
	class Client
	{
	public:
		bool connect(std::unique_ptr<IPEndpoint> ipEndpoint, std::shared_ptr<Client> client = nullptr);
		bool isConnected() const;
		bool frame();
	protected:
		std::unique_ptr<TCPConnection> m_connection;
		std::string m_connectionInfo = "";

		virtual void onConnectFail() const;
		virtual void onConnect(std::shared_ptr<Client> client = nullptr);
		virtual void onDisconnect(const std::string_view reason);
		virtual bool processPacket(Packet& packet);

	private:
		bool m_isConnected = false;
		pollfd m_master_fd = {};

		void closeConnection(std::string_view reason);
	};
}