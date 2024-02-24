#include "PacketManager.h"

namespace PNet
{
	bool PacketManager::hasPendingPacket() const
	{
		return !m_packets.empty();
	}

	void PacketManager::clear()
	{
		m_packets = {};
	}

	void PacketManager::append(std::shared_ptr<Packet> p)
	{
		m_packets.emplace(std::move(p));
	}

	std::shared_ptr<Packet> PacketManager::front()
	{
		return m_packets.front();
	}

	void PacketManager::popFront()
	{
		m_packets.pop();
	}
}