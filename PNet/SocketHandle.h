#pragma once
#define WIN32_LEAN_AND_MEAN
#include "WinSock2.h"

namespace PNet
{
	typedef SOCKET SocketHandle; //в линукс этот тип сразу является интом, поэтому используем typedef для мультиплатформенности
}