#include "TCPConnection.h"

namespace PNet
{
	TCPConnection::TCPConnection(Socket& socket, std::unique_ptr<IPEndpoint> ipEndpoint)
		: m_socket(socket), m_ipEndpoint(std::move(ipEndpoint))
	{
		m_connectionInfoForServer = '[' + m_ipEndpoint->getIPString() + ':' + std::to_string(m_ipEndpoint->getPort()) + ']';
	}

	std::string_view TCPConnection::getConnectionInfo() const
	{
		return m_connectionInfoForServer;
	}

	void TCPConnection::closeTCPConnection()
	{
		m_socket.close();
	}
}