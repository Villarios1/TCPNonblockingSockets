#pragma once

namespace PNet
{
	enum class SocketOption
	{
		TCP_NO_DELAY, // по умолч TCP_NODELAY =FALSE - делей есть - алгоритм Нейгла включен. Для отключения задержки нужно установить в TRUE
		IPV6_ONLY, // TRUE = только IPv6 может подключиться. FALSE - IPv6 и IPv4. По умолч = TRUE
	};
}