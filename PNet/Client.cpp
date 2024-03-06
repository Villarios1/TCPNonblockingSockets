#include "Client.h"

namespace PNet
{
	bool Client::connect(std::unique_ptr<IPEndpoint> ipEndpoint, std::shared_ptr<Client>  client)
	{
		m_isConnected = false;

		Socket socket = Socket(ipEndpoint->getIPVersion());
		if (socket.create())
		{
			std::cout << "Socket succesffully created." << std::endl;
			if (socket.setBlocking(true))
			{
				std::cout << "Connecting on port " << ipEndpoint->getPort() << "...\n";
				if (socket.connectSocket(*ipEndpoint, m_connectionInfo))
				{
					if (socket.setBlocking(false))
					{
						m_master_fd.fd = socket.getHandle();
						m_master_fd.events = POLLRDNORM;
						m_master_fd.revents = 0;

						m_connection = std::make_unique<TCPConnection>(socket, std::move(ipEndpoint)); //�� ������ ����� ���������� 2 ���� � ������������ TCP
						m_isConnected = true;														//����� ��� connection= ���������� � ���� + ������� TCP.
					}										//����� ������� ����� - ������ ������� copy. ��� ������� ������ TCP - ����� ����� ����� move.
				}		//� ����� ������ ����� 1 ����� ���, � ������ ����� ����� ���������� TCP=m_connection - �� ���� ���������� ����. �������� �� r-value ���.
			}			//����� ��� ����� � ���� ��������, ��� IPEndpoint.
			if (m_isConnected == false)
				socket.closeSocket();
		}
		m_isConnected ? onConnect(client) : onConnectFail();
		return m_isConnected;
	}

	void Client::onConnect(std::shared_ptr<Client> client)
	{
		std::cout << "Connection established: " << '[' << m_connectionInfo << "]\n";
	}

	void Client::onConnectFail() const
	{
		std::cerr << "Failed connection to server.\n";
	}

	bool Client::isConnected() const
	{
		return m_isConnected;
	}

	bool Client::frame()
	{
		if (m_connection->m_pmOutgoing->hasPendingPacket()) //���� � ���������� ���� ������� �� �������� ������
			m_master_fd.events = POLLRDNORM | POLLWRNORM; //������������� ��� ��� ������ ������ � ���� �������� ����� ��������� ��� � ��������

		const int result = WSAPoll(&m_master_fd, 1, 50);
		if (result == SOCKET_ERROR)
		{
			std::string reason = "WSAPoll error: " + std::to_string(WSAGetLastError());
			closeConnection(reason);
			return false;
		}
		if (result > 0) //���� �������� �����:
		{
			if (m_master_fd.revents & POLLERR)
			{
				closeConnection("Poll error (server disconnected)");
				return false;
			}
			else if (m_master_fd.revents & POLLHUP)
			{
				closeConnection("Poll hang up");
				return false;
			}
			else if (m_master_fd.revents & POLLNVAL)
			{
				closeConnection("Poll invalid socked");
				return false;
			}

			if (m_master_fd.revents & POLLRDNORM) //���� � ����� ��� send - �������� ������
			{
				PacketManager& pmi = *m_connection->m_pmIncoming; //�� ������� ���������� (� �������) ���������� ������ � ������� �� �����

				int bytesReceived = 0;
				if (pmi.m_cpTask == PacketTask::PROCESS_PACKET_SIZE)
					bytesReceived = recv(m_master_fd.fd, (char*)&pmi.m_cpSize + pmi.m_cpExtractionOffset, sizeof(uint16_t) - pmi.m_cpExtractionOffset, 0);
				else //process packet contents
					bytesReceived = recv(m_master_fd.fd, (char*)&m_connection->m_buffer + pmi.m_cpExtractionOffset, pmi.m_cpSize - pmi.m_cpExtractionOffset, 0);

				if (bytesReceived == 0)
				{
					closeConnection("Failed to recv: connection is closed");
					return false;
				}
				else if (bytesReceived == SOCKET_ERROR)
				{
					const int error = WSAGetLastError();
					if (error != WSAEWOULDBLOCK)
					{
						closeConnection("Socket error: " + error);
						return false;
					}
					//WSAEWOULDBLOCK �������� ��� recv �� �������� ����������� ���������, � ��� ����� ����������� ���������.
					//�� ��������� ����������, � ������ ��������� �� ��������� ��������.
					std::cerr << m_connectionInfo << "WSAEWOULDBLOCK on reading.\n";
				}

				if (bytesReceived > 0) //��� WSAEWOULDBLOCK bytesReceived ���� -1 //���� �������� 1 ����, �� ����� WSAEWOULDBLOCK? bytesReceived = -1 ??
					pmi.m_cpExtractionOffset += bytesReceived;

				if (pmi.m_cpTask == PacketTask::PROCESS_PACKET_SIZE)
				{
					if (pmi.m_cpExtractionOffset == sizeof(pmi.m_cpSize)) //���� ��������� �������� ������ ������
					{
						pmi.m_cpSize = ntohs(pmi.m_cpSize);
						if (pmi.m_cpSize > PNet::g_MaxPacketSize)
						{
							closeConnection("Packet size is too large.");
							return false;
						}
						pmi.m_cpExtractionOffset = 0;
						pmi.m_cpTask = PacketTask::PROCESS_PACKET_CONTENTS;
						//? connection.m_buffer.resize(connection.m_pmIncoming.m_cpSize);
					}
				}
				else //process packet contents
				{
					if (pmi.m_cpExtractionOffset == pmi.m_cpSize) //���� ��������� �������� �����
					{
						std::shared_ptr<Packet> packet = std::make_shared<Packet>();
						packet->m_buffer.resize(pmi.m_cpSize);
						memcpy(&packet->m_buffer[0], m_connection->m_buffer, pmi.m_cpSize); // ?

						pmi.append(packet);

						pmi.m_cpExtractionOffset = 0;
						pmi.m_cpSize = 0;
						pmi.m_cpTask = PacketTask::PROCESS_PACKET_SIZE;
					}
				}
			}

			if (m_master_fd.revents & POLLWRNORM) //������������� ������ ���������� � ������ ������
			{
				PacketManager& pmo = *m_connection->m_pmOutgoing;
				while (pmo.hasPendingPacket()) //��������� ���� �� ��� ����������. � �������� ��������� ��������� ��� ��������� ������
				{
					int bytesSended = 0;
					if (pmo.m_cpTask == PacketTask::PROCESS_PACKET_SIZE)
					{
						pmo.m_cpSize = static_cast<uint16_t>(pmo.front()->m_buffer.size());
						const uint16_t encodedPacketSize = htons(pmo.m_cpSize);

						bytesSended = send(m_master_fd.fd, (char*)&encodedPacketSize + pmo.m_cpExtractionOffset, sizeof(uint16_t) - pmo.m_cpExtractionOffset, 0);
						if (bytesSended > 0) //��� WSAEWOULDBLOCK bytesSended ���� -1 //���� �������� 1 ����, �� ����� WSAEWOULDBLOCK? bytesSended = -1 ??
							pmo.m_cpExtractionOffset += bytesSended;

						if (pmo.m_cpExtractionOffset == sizeof(uint16_t)) //���� ������ ��������� �������
						{
							pmo.m_cpExtractionOffset = 0;
							pmo.m_cpTask = PacketTask::PROCESS_PACKET_CONTENTS;
						}
					}
					else //process packet contents
					{
						bytesSended = send(m_master_fd.fd, &pmo.front()->m_buffer[0] + pmo.m_cpExtractionOffset, pmo.m_cpSize - pmo.m_cpExtractionOffset, 0);
						if (bytesSended > 0) //��� WSAEWOULDBLOCK bytesSended ���� -1 //���� �������� 1 ����, �� ����� WSAEWOULDBLOCK? bytesSended = -1 ??
							pmo.m_cpExtractionOffset += bytesSended;

						if (pmo.m_cpExtractionOffset == pmo.m_cpSize) //���� ����� ��������� �������
						{
							pmo.m_cpExtractionOffset = 0;
							pmo.m_cpTask = PacketTask::PROCESS_PACKET_SIZE;
							pmo.popFront();
						}
					}

					if (bytesSended == SOCKET_ERROR)
					{
						const int error = WSAGetLastError();
						if (error != WSAEWOULDBLOCK)
						{
							closeConnection("Socket error: " + error);
							return false;
						}//��� send ����� �����, � non-blocking �� ���� ���������� ������� - ��������� � ��������� ������
						std::cerr << m_connectionInfo << "WSAEWOULDBLOCK on writing. Breaking.\n";
						break; //����� ����� ����� ���������� �������� �� ���������� ����� - ����������� ���������
					}
				}
				if (pmo.hasPendingPacket() == false)
					m_master_fd.events = POLLRDNORM;
			}
			m_master_fd.revents = 0;
		}
		//����� ��������� ���� ������� � ������� ��������
		while (m_connection->m_pmIncoming->hasPendingPacket()) //������������ ��� ��������
		{
			if (!processPacket(*m_connection->m_pmIncoming->front()))
			{
				closeConnection("Failed to process packet.");
				return false;
			}
			m_connection->m_pmIncoming->popFront();
		}
		return true;
	}

	bool Client::processPacket(Packet& packet)
	{
		std::cout << "Packet received with size: " << packet.m_buffer.size() << '\n';

		return true;
	}

	void Client::onDisconnect(const std::string_view reason)
	{
		std::cerr << "Lost connection. Reason: " << reason << ".\n";
	}

	void  Client::closeConnection(std::string_view reason)
	{
		onDisconnect(reason);
		m_master_fd = {};
		m_isConnected = false;
		m_connection->closeTCPConnection();
	}
}