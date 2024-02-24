#pragma once
#define WIN32_LEAN_AND_MEAN
#include "WinSock2.h"

namespace PNet
{
	class Network
	{
	public:
		static bool initialize();
		static void shutdown();
	};
}