#pragma once
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include "WinSock2.h"
#endif //в Linux этот заголовок не находит, но он там не нужен.

namespace PNet
{
	class Network
	{
	public:
		static bool initialize();
		static void shutdown();
	};
}