#pragma once
#define WIN32_LEAN_AND_MEAN
#include "WinSock2.h"
#include "PacketType.h"
#include <vector>
#include <filesystem>
#include <concepts>

namespace PNet
{
	class Packet
	{
	private:
		uint32_t m_extractionOffset;
	public:
		std::vector<char> m_buffer; //хранит в network порядке

	public:
		Packet(const PacketType packetType = PacketType::PACKET_INVALID);

		template <typename T> requires std::is_same<T, bool>::value  //Packet& operator<<(bool data); ломает Packet& operator>>(uint32_t& data); лол
		Packet& operator<<(T data)
		{
			append(&data, sizeof(data));
			return *this;
		}
		Packet& operator>>(bool& data);

		Packet& operator<<(uint32_t data);
		Packet& operator>>(uint32_t& data);

		Packet& operator<<(std::string data);
		Packet& operator>>(std::string& data);

		template<typename T> requires std::is_same<T, std::filesystem::path>::value
		Packet& operator<<(T data)
		{
			*this << data.string();

			return *this;
		}
		Packet& operator>>(std::filesystem::path& data);

		PacketType getPacketType();
	private:
		void assignPacketType(const PacketType packetType);
		void append(const void* data, uint32_t size);
		void reset();
	};
}