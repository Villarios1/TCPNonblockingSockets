#include "Packet.h"
#include "Constants.h"
#include "PacketException.h"

namespace PNet
{
	Packet::Packet(const PacketType packetType)
	{
		reset();
		assignPacketType(packetType);
	}

	void Packet::reset() //обнуление и назначение в первые 2 байта неизвестный тип пакета
	{
		m_buffer.resize(sizeof(PacketType));
		assignPacketType(PacketType::PACKET_INVALID);
		m_extractionOffset = sizeof(PacketType);
	}

	void Packet::assignPacketType(const PacketType packetType) // назначаем в первые 2 байта тип пакета в network order
	{
		PacketType* packetTypePtr = reinterpret_cast<PacketType*>(&m_buffer[0]);
		const u_short encodedPacket = htons(static_cast<u_short>(packetType));
		*packetTypePtr = static_cast<PacketType>(encodedPacket);
	}
	
	PacketType Packet::getPacketType() //возврат типа пакета в host order из первых двух байт
	{
		const PacketType* packetTypePtr = reinterpret_cast<PacketType*>(&m_buffer[0]);
		const u_short packetType = ntohs(static_cast<u_short>(*packetTypePtr));
		return static_cast<PacketType>(packetType);
	}

	void Packet::append(const void* data, uint32_t size)
	{
		if ((m_buffer.size() + size) > g_MaxPacketSize) // проверка одного ввода
			throw PacketException("[Packet::append(const void*, uint32_t)] - Packet size exceeded max packet size.");

		m_buffer.insert(m_buffer.end(), (char*)data, (char*)data + size);
	}

	Packet& Packet::operator<<(uint32_t data) // buffer << data
	{
		data = htonl(data);
		append(&data, sizeof(data));
		return *this;
	}

	Packet& Packet::operator<<(std::string data) // const string s = "Hello"; packet << string&
	{
		*this << static_cast<uint32_t>(data.size());

		append(data.data(), static_cast<uint32_t>(data.length()));

		return *this;
	}

	Packet& Packet::operator>>(bool& data)
	{
		if ((m_extractionOffset + sizeof(data)) > m_buffer.size())
			throw PacketException("[Packet& Packet::operator>>(bool&)] - Exctraction offset exceeded buffer size.");

		data = m_buffer[m_extractionOffset];
		m_extractionOffset += sizeof(data);

		return *this;
	}

	Packet& Packet::operator>>(uint32_t& data) // buffer >> data // вывод не изменяет buffer
	{
		if ((m_extractionOffset + sizeof(data)) > m_buffer.size())
			throw PacketException("[Packet& Packet::operator>>(uint32_t&)] - Exctraction offset exceeded buffer size.");

		data = *reinterpret_cast<uint32_t*>(&m_buffer[m_extractionOffset]);
		data = ntohl(data);
		m_extractionOffset += sizeof(data);

		return *this;
	}

	Packet& Packet::operator>>(std::string& data) // string s; packet >> string&
	{
		data.clear();

		uint32_t length;
		*this >> length;
		if ((m_extractionOffset + length) > m_buffer.size())
			throw PacketException("[Packet& Packet::operator>>(std::string&)] - Exctraction offset exceeded buffer size.");

		data.resize(length);
		data.assign(&m_buffer[m_extractionOffset], length);
		m_extractionOffset += length;

		return *this;
	}

	Packet& Packet::operator>>(std::filesystem::path& data)
	{
		data.clear();

		uint32_t length;
		*this >> length;
		if ((m_extractionOffset + length) > m_buffer.size())
			throw PacketException("[Packet& Packet::operator>>(std::filesystem::path&)] - Exctraction offset exceeded buffer size.");

		std::string buf(&m_buffer[m_extractionOffset], length);
		data.assign(buf);
		m_extractionOffset += length;

		return *this;
	}
}