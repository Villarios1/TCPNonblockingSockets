#pragma once
#include "PacketType.h"

namespace PNet
{
	const uint16_t g_MaxPacketSize = 16384; //max 65535. Рекомендуется > 1000 т.к сообщения не разбиваются на несколько пакетов

	//пакет с куском файла состоит из типа пакета + номер отправляемого чанка + размера буфера + самого буфера
	const uint16_t g_MaxChunkSize = g_MaxPacketSize - sizeof(PacketType) - sizeof(uint32_t) - sizeof(uint32_t);

	const uint64_t g_MaxFileSize = static_cast<uint64_t>(6e9); //1.1e12 = 1TB, 2.9e10 = 27GB, 4.3e9 = 4GB
}