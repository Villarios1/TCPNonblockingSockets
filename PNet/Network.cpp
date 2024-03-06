#include "Network.h"
#include <iostream>

namespace PNet
{
	bool Network::initialize()
	{
		#ifdef _WIN32 //в Linux не нужно
		WSAData wsaData;

		const int startup = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (startup != 0)
		{
			std::cerr << "WSAStartup failed with error: " << startup << std::endl;
			return false;
		}

		if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
		{
			std::cerr << "Could not find a usable version of the WinSock.dll." << std::endl;
			return false;
		}
		#endif
		return true;
	}

	void Network::shutdown()
	{
		#ifdef _WIN32
		if (WSACleanup() != 0)
			std::cerr << "WSACleanup error: " << WSAGetLastError() << std::endl;
		#endif
	}
}