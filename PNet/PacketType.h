#pragma once
#include <stdint.h>

namespace PNet
{
	enum class PacketType : uint16_t
	{
		PACKET_INVALID,
		PACKET_CLIENT_VERSION,
		PACKET_SERVER_MESSAGE,
		PACKET_CLIENT_IDENTIFIER,
		PACKET_CLIENT_MESSAGE,
		PACKET_FILE_INFO,
		PACKET_FILE_CHUNK,
		PACKET_UPLOAD_PERMISSION,
		PACKET_INTEGER_ARRAY
	};
}