#pragma once
#include "SocketHandle.h"
#include "IPVersion.h"
#include "SocketOption.h"
#include "IPEndpoint.h"
#include "Constants.h"
#include "Packet.h"
#include "string_view"

namespace PNet
{
	class Socket
	{
	private:
		IPVersion m_ipVersion;
		SocketHandle m_handle;
	public:
		Socket(IPVersion ipVersion = IPVersion::IPV4, 
			SocketHandle handle = INVALID_SOCKET);

		bool create();
		bool setBlocking(const bool isBlocking) const;
		bool listenSocket(const IPEndpoint& ipEndpoint, const int maxConnections = 10) const;
		bool acceptSocket(Socket& outSocket, IPEndpoint* optoutEndpoint = nullptr) const;
		bool connectSocket(const IPEndpoint& ipEndpoint, std::string& outServerInfo) const;
		bool close();

		SocketHandle getHandle() const;
		IPVersion getIPVersion() const;

		void printErrorDescription(const std::string_view where, const int error) const;
	private:
		bool setSocketOption(const SocketOption option, const BOOL value) const;
		bool bindSocket(const IPEndpoint& ipEndpoint) const;
		bool getServerInfo(std::string& outServerInfo) const;
	};
}