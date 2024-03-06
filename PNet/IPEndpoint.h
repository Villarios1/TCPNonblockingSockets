#pragma once
#include "IPVersion.h"
#include <string>
#include <vector>

#ifdef _WIN32
#include <WS2tcpip.h>
#else // Linux
#include <arpa/inet.h> //inet_pton + in.h
#define WSAGetLastError() errno
#include <netdb.h> //getaddrinfo
#include <string.h> //memcpy Linux
#endif

namespace PNet
{
	class IPEndpoint
	{
		IPVersion m_ipVersion = IPVersion::UNKNOWN;
		std::string m_hostname = "";
		std::string m_ipString = "";
		std::vector<uint8_t> m_ipBytes; // network order
		unsigned short m_port = 0;

	public:
		IPEndpoint(const char* ip, const unsigned short port);
		IPEndpoint(sockaddr* addr);
		IPEndpoint() {};

		sockaddr_in getSockaddrIPv4() const;
		sockaddr_in6 getSockaddrIPv6() const;
		IPVersion getIPVersion() const;
		std::string_view getHostname() const;
		const std::string& getIPString() const;
		unsigned short getPort() const;
	};
}