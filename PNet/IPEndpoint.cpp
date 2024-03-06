#include "IPEndpoint.h"
#include <assert.h>
#include <iostream>

namespace PNet
{
	// ������� ���������� ��� ����������� �� ���������� ip
	IPEndpoint::IPEndpoint(const char* ip, const unsigned short port)
	{
		m_port = port;

		// IPv4 ?
		in_addr addr;
		int result = inet_pton(AF_INET, ip, &addr);
		if (result == 1) // ���� ������ IPv4 �����
		{
			m_ipString = ip;
			m_hostname = ip;
			m_ipVersion = IPVersion::IPV4;
			m_ipBytes.resize(sizeof(addr));
			memcpy(&m_ipBytes[0], &addr, sizeof(addr));

			return;
		}

		// IPv6 ?
		in6_addr addr6;
		result = inet_pton(AF_INET6, ip, &addr6);
		if (result == 1) // ���� ������ IPv6 �����
		{
			m_ipString = ip;
			m_hostname = ip;
			m_ipVersion = IPVersion::IPV6;
			m_ipBytes.resize(sizeof(addr6));
			memcpy(&m_ipBytes[0], &addr6, sizeof(addr6));

			return;
		}

		// hostname ?(e.g www.google.com)
		addrinfo hints = {};
		hints.ai_family = AF_UNSPEC;
		addrinfo* hostinfo = nullptr;
		if (getaddrinfo(ip, NULL, &hints, &hostinfo) == 0)
		{
			m_hostname = ip;

			if (hostinfo->ai_family == AF_INET) //hostname to IPv4 
			{
				m_ipVersion = IPVersion::IPV4;

				const sockaddr_in* host_addr = reinterpret_cast<sockaddr_in*>(hostinfo->ai_addr);

				const uint8_t ipv4_length = 16;
				m_ipString.resize(ipv4_length);
				m_ipString = inet_ntop(AF_INET, &host_addr->sin_addr, &m_ipString[0], ipv4_length);

				m_ipBytes.resize(sizeof(in_addr));
				memcpy(&m_ipBytes[0], &host_addr->sin_addr, sizeof(in_addr));
			}
			else //hostname to IPv6
			{
				m_ipVersion = IPVersion::IPV6;

				const sockaddr_in6* host_addr6 = reinterpret_cast<sockaddr_in6*>(hostinfo->ai_addr);

				const uint8_t ipv6_length = 46;
				m_ipString.resize(ipv6_length);
				m_ipString = inet_ntop(AF_INET6, &host_addr6->sin6_addr, &m_ipString[0], ipv6_length);

				m_ipBytes.resize(sizeof(in6_addr));
				memcpy(&m_ipBytes[0], &host_addr6->sin6_addr, sizeof(in6_addr));
			}

			freeaddrinfo(hostinfo);
			return;
		}
		else
			std::cerr << "[IPEndpoint(const char*, const unsigned short)] getaddrinfo error: " << WSAGetLastError() << '\n';
	}

	IPEndpoint::IPEndpoint(sockaddr* addr)
	{
		assert(addr->sa_family == AF_INET || addr->sa_family == AF_INET6);

		if (addr->sa_family == AF_INET) //IPv4
		{
			const sockaddr_in* ipv4addr = reinterpret_cast<sockaddr_in*>(addr);

			m_ipVersion = IPVersion::IPV4;
			m_port = ntohs(ipv4addr->sin_port);

			m_ipBytes.resize(sizeof(in_addr));
			memcpy(&m_ipBytes[0], &ipv4addr->sin_addr, sizeof(in_addr));

			const uint8_t ipv4_len = 16;
			char ipv4_buf[ipv4_len];
			const char* ipv4_string = inet_ntop(AF_INET, &ipv4addr->sin_addr, ipv4_buf, ipv4_len);
			if (ipv4_string != NULL)
			{
				m_ipString = ipv4_string;
				m_hostname = ipv4_string;
			}
			else
				std::cerr << "[IPEndpoint::IPEndpoint(sockaddr*)] inet_ntop ipv4 error: " << WSAGetLastError() << '\n';
		}
		else //IPv6
		{
			const sockaddr_in6* ipv6addr = reinterpret_cast<sockaddr_in6*>(addr);

			m_ipVersion = IPVersion::IPV6;
			m_port = ntohs(ipv6addr->sin6_port);

			m_ipBytes.resize(sizeof(in6_addr));
			memcpy(&m_ipBytes[0], &ipv6addr->sin6_addr, sizeof(in6_addr));

			const uint8_t ipv6_len = 46;
			char ipv6_buf[ipv6_len];
			const char* ipv6_string = inet_ntop(AF_INET6, &ipv6addr->sin6_addr, ipv6_buf, ipv6_len);
			if (ipv6_string != NULL)
			{
				m_ipString = ipv6_string;
				m_hostname = ipv6_string;
			}
			else
				std::cerr << "[IPEndpoint::IPEndpoint(sockaddr*)] inet_ntop error: " << WSAGetLastError() << '\n';
		}
	}

	sockaddr_in IPEndpoint::getSockaddrIPv4() const
	{
		assert(m_ipVersion == IPVersion::IPV4);

		sockaddr_in addr = {};
		memcpy(&addr.sin_addr, &m_ipBytes[0], sizeof(in_addr));
		addr.sin_port = htons(m_port);
		addr.sin_family = AF_INET;

		return addr;
	}

	sockaddr_in6 IPEndpoint::getSockaddrIPv6() const
	{
		assert(m_ipVersion == IPVersion::IPV6);

		sockaddr_in6 addr = {};
		memcpy(&addr.sin6_addr, &m_ipBytes[0], sizeof(in6_addr));
		addr.sin6_port = htons(m_port);
		addr.sin6_family = AF_INET6;

		return addr;
	}

	IPVersion IPEndpoint::getIPVersion() const
	{
		return m_ipVersion;
	}

	std::string_view IPEndpoint::getHostname() const
	{
		return m_hostname;
	}

	const std::string& IPEndpoint::getIPString() const
	{
		return m_ipString;
	}

	unsigned short IPEndpoint::getPort() const
	{
		return m_port;
	}
}