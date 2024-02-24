#pragma once
#include "Packet.h"
#include <queue>
#include <memory>

namespace PNet
{
	enum class PacketTask
	{
		PROCESS_PACKET_SIZE,
		PROCESS_PACKET_CONTENTS
	};

	class PacketManager
	{
		std::queue<std::shared_ptr<Packet>> m_packets;
	public:
		bool hasPendingPacket() const;
		void append(std::shared_ptr<Packet> p);
		std::shared_ptr<Packet> front();
		void popFront();
		void clear();

		//current packet
		uint16_t m_cpSize = 0;
		int m_cpExtractionOffset = 0;
		PacketTask m_cpTask = PacketTask::PROCESS_PACKET_SIZE;
	};
}