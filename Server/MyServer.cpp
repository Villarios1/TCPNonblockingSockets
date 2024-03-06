#include "MyServer.h"
#include <sstream>

void MyServer::onConnect(TCPConnection& newConnection)
{
	std::cout << "New connection accepted: " << newConnection.getConnectionInfo() << '\n';

	std::shared_ptr<Packet> welcomeMessagePacket = std::make_shared<Packet>(PacketType::PACKET_SERVER_MESSAGE);
	*welcomeMessagePacket << "Welcome to the server!";
	newConnection.m_pmOutgoing->append(welcomeMessagePacket);

	std::shared_ptr<Packet> connectedPacket = std::make_shared<Packet>(PacketType::PACKET_SERVER_MESSAGE);
	std::string connectedString = "New user connected: ";
	connectedString.append(newConnection.getConnectionInfo().data());
	*connectedPacket << connectedString;
	for (TCPConnection& connection : m_connections)
	{
		if (&connection == &newConnection)
			continue;

		connection.m_pmOutgoing->append(connectedPacket);
	}
}

void MyServer::onDisconnect(const TCPConnection& lostConnection, const std::string_view reason)
{
	std::cerr << '[' << reason << "] Connection lost: " << lostConnection.getConnectionInfo() << ".\n";

	std::shared_ptr<Packet> disconnectedPacket = std::make_shared<Packet>(PacketType::PACKET_SERVER_MESSAGE);
	std::string disconnectedString = "A user disconnected: ";
	disconnectedString.append(lostConnection.getConnectionInfo().data());
	*disconnectedPacket << disconnectedString;
	for (TCPConnection& connection : m_connections)
	{
		if (&connection == &lostConnection)
			continue;

		connection.m_pmOutgoing->append(disconnectedPacket);
	}
}

bool MyServer::processPacket(TCPConnection& fromConnection, std::shared_ptr<Packet> packet)
{
	switch (packet->getPacketType())
	{
	case PacketType::PACKET_CLIENT_VERSION:
	{
		uint32_t majorVersion;
		uint32_t minorVersion;
		*packet >> majorVersion >> minorVersion;
		std::stringstream sstream;
		sstream << "Your version is " << majorVersion << '.' << minorVersion <<
			". Latest version of program is " << m_latestVersionMajor << '.' << m_latestVersionMinor;

		std::shared_ptr<Packet> vVerificationPacket = std::make_shared<Packet>(PacketType::PACKET_CLIENT_VERSION);
		*vVerificationPacket << sstream.str();
		fromConnection.m_pmOutgoing->append(vVerificationPacket);
		break;
	}
	case PacketType::PACKET_FILE_INFO:
	{
		bool allow_to_upload = (m_connections.size() == 1) ? false : true;
		std::shared_ptr<Packet> permissionPacket = std::make_shared<Packet>(PacketType::PACKET_UPLOAD_PERMISSION);
		*permissionPacket << allow_to_upload; //первым пакетом устанавливаем отказ/разрешение на отправку тела
		fromConnection.m_pmOutgoing->append(permissionPacket);

		if (!allow_to_upload) //в случае отказа будет второй пакет с сообщением
		{
			std::shared_ptr<Packet> refusedPacket = std::make_shared<Packet>(PacketType::PACKET_SERVER_MESSAGE);
			*refusedPacket << "There are no more connected clients. Forwarding refused.";
			fromConnection.m_pmOutgoing->append(refusedPacket);
			break;
		}

		*packet << fromConnection.getConnectionInfo().data(); //иначе дополняем заголовок информацией об отправителе
		for (TCPConnection& connection : m_connections) //и рассылаем всем клиентам
		{
			if (&connection == &fromConnection)
				continue;
			connection.m_pmOutgoing->append(packet);
		}
		break;
	}
	case PacketType::PACKET_CLIENT_MESSAGE:
	{
		std::shared_ptr<Packet> clientMessageHeaderPacket = std::make_shared<Packet>(PacketType::PACKET_CLIENT_IDENTIFIER);
		*clientMessageHeaderPacket << fromConnection.getConnectionInfo().data();
		for (TCPConnection& connection : m_connections)
		{
			if (&connection == &fromConnection)
				continue;

			connection.m_pmOutgoing->append(clientMessageHeaderPacket); //первый пакет - IP:Port
			connection.m_pmOutgoing->append(packet); //второй - сообщение
		}
		break;
	}
	case PacketType::PACKET_FILE_CHUNK:
	case PacketType::PACKET_INTEGER_ARRAY:
	{
		for (TCPConnection& connection : m_connections) //просто пересылка всем
		{
			if (&connection == &fromConnection)
				continue;
			connection.m_pmOutgoing->append(packet);
		}
		break;
	}
	default:
		std::cerr << "Unrecognized packet type.\n";
		return false;
	}

	return true;
}