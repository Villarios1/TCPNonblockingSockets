#include "Socket.h"
#include <assert.h>
#include <iostream>
#include <any>

#ifndef _WIN32
#include <unistd.h> //closesocket Linux
#include <sys/ioctl.h>
#include <sys/socket.h> //getsockname, setsockopt
#include <netinet/tcp.h> //TCP_NODELAY
#define ioctlsocket ioctl
#endif

namespace PNet
{
	Socket::Socket(IPVersion ipVersion, SocketHandle handle) : m_ipVersion(ipVersion), m_handle(handle)
	{
		assert(m_ipVersion == IPVersion::IPV4 || m_ipVersion == IPVersion::IPV6);
	}

	bool Socket::create()
	{
		assert(m_ipVersion == IPVersion::IPV4 || m_ipVersion == IPVersion::IPV6); //�����

		if (m_handle != INVALID_SOCKET) //���� handle ��� ���������
		{
			std::cerr << "create socket error: INVALID_SOCKET\n";
			return false;
		}

		m_handle = socket((m_ipVersion == IPVersion::IPV4) ? AF_INET : AF_INET6, SOCK_STREAM, IPPROTO_TCP);
		if (m_handle == INVALID_SOCKET)
		{
			printErrorDescription(__func__, WSAGetLastError());
			return false;
		}

		if (!setBlocking(false))
			return false;

		if (!setSocketOption(SocketOption::TCP_NO_DELAY, true))
			return false;

		return true;
	}

	bool Socket::bindSocket(const IPEndpoint& ipEndpoint) const
	{
		if (m_handle == INVALID_SOCKET)
		{
			std::cerr << "bindSocket error: INVALID_SOCKET\n";
			return false;
		}
		assert(m_ipVersion == ipEndpoint.getIPVersion());

		int result = SOCKET_ERROR;
		if (m_ipVersion == IPVersion::IPV4)
		{
			const sockaddr_in addr = ipEndpoint.getSockaddrIPv4();
			result = bind(m_handle, (sockaddr*)&addr, sizeof(addr));
		}
		else //IPv6
		{
			const sockaddr_in6 addr = ipEndpoint.getSockaddrIPv6();
			result = bind(m_handle, (sockaddr*)&addr, sizeof(addr));
		}

		if (result == SOCKET_ERROR)
		{
			printErrorDescription(__func__, WSAGetLastError());
			return false;
		}

		return true;
	}

	bool Socket::listenSocket(const IPEndpoint& ipEndpoint, const int maxConnections) const
	{
		if (m_ipVersion == IPVersion::IPV6)
		{
			if (!setSocketOption(SocketOption::IPV6_ONLY, false))
				return false;
		}

		if (!bindSocket(ipEndpoint))
			return false;

		const int result = listen(m_handle, maxConnections);
		if (result == SOCKET_ERROR)
		{
			printErrorDescription(__func__, WSAGetLastError());
			return false;
		}

		return true;
	}

	bool Socket::acceptSocket(Socket& outSocket, IPEndpoint* optoutEndpoint) const
	{
		assert(m_ipVersion == IPVersion::IPV4 || m_ipVersion == IPVersion::IPV6);

		std::any accepted_addr;
		#ifdef _WIN32
		int addr_length;
		#else
		socklen_t addr_length;
		#endif
		if (m_ipVersion == IPVersion::IPV4)
		{
			const sockaddr_in addr = {};
			accepted_addr = addr;
			addr_length = sizeof(sockaddr_in);
		}
		else //IPv6
		{
			const sockaddr_in6 addr = {};
			accepted_addr = addr;
			addr_length = sizeof(sockaddr_in6);
		}
		const SocketHandle acceptedConnection = accept(m_handle, (sockaddr*)&accepted_addr, &addr_length);
		if (acceptedConnection == INVALID_SOCKET)
		{
			printErrorDescription(__func__, WSAGetLastError());
			return false;
		}

		outSocket = Socket(m_ipVersion, acceptedConnection);
		if(optoutEndpoint)
			*optoutEndpoint = IPEndpoint((sockaddr*)&accepted_addr); //client info

		return true;
	}

	bool Socket::connectSocket(const IPEndpoint& ipEndpoint, std::string& outServerInfo) const
	{
		assert(m_ipVersion == ipEndpoint.getIPVersion());

		int result = SOCKET_ERROR;
		if (m_ipVersion == IPVersion::IPV4)
		{
			const sockaddr_in addr = ipEndpoint.getSockaddrIPv4();
			result = connect(m_handle, (sockaddr*)&addr, sizeof(addr));
		}
		else //IPv6
		{
			const sockaddr_in6 addr = ipEndpoint.getSockaddrIPv6();
			result = connect(m_handle, (sockaddr*)&addr, sizeof(addr));
		}

		if (result == SOCKET_ERROR)
		{
			printErrorDescription(__func__, WSAGetLastError());
			return false;
		}

		if (!getServerInfo(outServerInfo))
			return false;
//segment fault right there
		return true;
	}

	bool Socket::getServerInfo(std::string& outServerInfo) const
	{
		std::any connection_addr;
		int result = SOCKET_ERROR;
		if (m_ipVersion == IPVersion::IPV4)
		{
			sockaddr_in ipv4addr = {};
			uint32_t addr_size = sizeof(ipv4addr);
			connection_addr = ipv4addr;
			result = getsockname(m_handle, (sockaddr*)&connection_addr, &addr_size);
		}
		else // IPv6
		{
			sockaddr_in6 ipv6addr = {};
			uint32_t addr_size = sizeof(ipv6addr);
			connection_addr = ipv6addr;
			result = getsockname(m_handle, (sockaddr*)&connection_addr, &addr_size);
		}

		if (result == SOCKET_ERROR)
		{
			printErrorDescription(__func__, WSAGetLastError());
			return false;
		}

		const IPEndpoint serverEndpoint = ((sockaddr*)&connection_addr);
		outServerInfo = serverEndpoint.getIPString() + ':' + std::to_string(serverEndpoint.getPort());

		return true;
	}

	bool Socket::closeSocket()
	{
		if (m_handle == INVALID_SOCKET)
		{
			std::cerr << "Close error: INVALID_SOCKET.\n";
			return false;
		}

		if (closesocket(m_handle) == SOCKET_ERROR)
		{
			printErrorDescription(__func__, WSAGetLastError());
			return false;
		}

		m_handle = INVALID_SOCKET;

		return true;
	}

	SocketHandle Socket::getHandle() const
	{
		return m_handle;
	}

	IPVersion Socket::getIPVersion() const
	{
		return m_ipVersion;
	}
	
	bool Socket::setSocketOption(const SocketOption option, const uint32_t value) const
	{
		int result = 0;

		switch (option)
		{
		case SocketOption::TCP_NO_DELAY: 
			result = setsockopt(m_handle, IPPROTO_TCP, TCP_NODELAY, (const char*)&value, sizeof(value));
			break;
		case SocketOption::IPV6_ONLY:
			result = setsockopt(m_handle, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&value, sizeof(value));
			break;
		default:
			std::cerr << "Unkown socket option.\n";
			return false;
		}

		if (result != 0)
		{
			printErrorDescription(__func__, WSAGetLastError());
			return false;
		}

		return true;
	}

	bool Socket::setBlocking(const bool isBlocking) const
	{
		u_long nonBlocking = 1;
		u_long blocking = 0;
		const int result = ioctlsocket(m_handle, FIONBIO, isBlocking ? &blocking : &nonBlocking);
		if (result == SOCKET_ERROR)
		{
			printErrorDescription(__func__, WSAGetLastError());
			return false;
		}

		return true;
	}

	void Socket::printErrorDescription(const std::string_view where, const int error) const
	{
		std::cerr << "An error occured in " << where << ": ";
		switch (error)
		{
		case 98: //Linux
		case 10048: std::cerr << "The address is already in use.\n"; break;
		case 10054: std::cerr << "Connection was terminated by remote node.\n"; break;
		case 111: //Linux
		case 10061: std::cerr << "Connection refused.\n"; break;
		default: std::cerr << "Unknown errorcode: " << error << '\n'; break;
		}
	}
}