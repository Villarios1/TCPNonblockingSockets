#include "Server.h"
#include "Network.h"

namespace PNet
{
	bool Server::initialize(std::unique_ptr<IPEndpoint> ipEndpoint)
	{
		m_master_fd.clear();
		m_connections.clear();

		m_listeningSocket = Socket(ipEndpoint->getIPVersion());
		if (!m_listeningSocket.create())
		{
			std::cerr << "Socket failed to create.\n";
			return false;
		}
		std::cout << "Socket successfully created.\n";

		if (!m_listeningSocket.listenSocket(*ipEndpoint))
		{
			std::cerr << "Failed to listen socket.\n";
			m_listeningSocket.closeSocket();
			return false;
		}
		std::cout << "Socket listening a " << ipEndpoint->getPort() << " port\n\n";

		pollfd listeningSocketFD;
		listeningSocketFD.fd = m_listeningSocket.getHandle();
		listeningSocketFD.events = POLLRDNORM;
		listeningSocketFD.revents = 0;
		m_master_fd.push_back(listeningSocketFD); //������ ��������� - ����� ������� (���������)

		return true;
	}

	void Server::frame()
	{
		for (int i = 0; i < static_cast<int>(m_connections.size()); ++i)
			if (m_connections[i].m_pmOutgoing->hasPendingPacket()) //���� � ���������� ���� ������� �� �������� ������
				m_master_fd[i + 1].events = POLLRDNORM | POLLWRNORM; //������������� ��� ��� ������ ������ � ���� �������� ����� ��������� ��� � ��������

		const int result = WSAPoll(m_master_fd.data(), static_cast<uint32_t>(m_master_fd.size()), 50);
		if (result == SOCKET_ERROR)
		{
			m_listeningSocket.printErrorDescription(__func__, WSAGetLastError());
			m_listeningSocket.closeSocket();
			return;
		}
		if (result > 0) //���� �������� �����:
		{
#pragma region listener
			pollfd& listeningSocketFD = m_master_fd[0];
			if (listeningSocketFD.revents & POLLRDNORM) //���� ���-�� �������� connect � ���������� ������ - � ���� ���� ��� ������� - ��������� �����������
			{
				listeningSocketFD.revents = 0;

				Socket newConnectionSocket; //����� �� �������, ���������� �� ����� � ����� ��������
				std::unique_ptr<IPEndpoint> newConnectionEndpoint = std::make_unique<IPEndpoint>();
				if (!m_listeningSocket.acceptSocket(newConnectionSocket, newConnectionEndpoint.get()))
				{
					m_listeningSocket.closeSocket();
					return;
				}
//segment fault
				pollfd newConnectionFD = {};
				newConnectionFD.fd = newConnectionSocket.getHandle();
				newConnectionFD.events = POLLRDNORM; //������������� � ����� ������ �������� �� ������� ������
				newConnectionFD.revents = 0;
				m_master_fd.push_back(newConnectionFD);
				m_connections.emplace_back(newConnectionSocket, std::move(newConnectionEndpoint));

				TCPConnection& acceptedConnection = m_connections[m_connections.size() - 1];
				onConnect(acceptedConnection);
			}
#pragma endregion Code spesific to the listening socket
			for (int i = static_cast<int>(m_master_fd.size()) - 1; i >= 1; --i) //� ��������� ������� � ������ ������ ��� ��������������:
			{
				const int connectionIndex = i - 1;
				TCPConnection& connection = m_connections[connectionIndex];

				if (m_master_fd[i].revents & POLLERR)
				{
					closeConnection(connectionIndex, "Poll error (client disconnected)");
					continue;
				}
				else if (m_master_fd[i].revents & POLLHUP)
				{
					closeConnection(connectionIndex, "Poll hang up");
					continue;
				}
				else if (m_master_fd[i].revents & POLLNVAL)
				{
					closeConnection(connectionIndex, "Poll invalid socked");
					continue;
				}

				pollfd& currentConnectionFD = m_master_fd[i];
				if (currentConnectionFD.revents & POLLRDNORM) //���� � ����� ��� send - ���� ��� ������� - �������� ������
				{
					PacketManager& pmi = *connection.m_pmIncoming; //�� ������� ���������� (� �������) ���������� ������ � ������� �� �����

					int bytesReceived = 0;
					if (pmi.m_cpTask == PacketTask::PROCESS_PACKET_SIZE)
						bytesReceived = recv(m_master_fd[i].fd, (char*)&pmi.m_cpSize + pmi.m_cpExtractionOffset, sizeof(uint16_t) - pmi.m_cpExtractionOffset, 0);
					else //process packet contents
						bytesReceived = recv(m_master_fd[i].fd, (char*)&connection.m_buffer + pmi.m_cpExtractionOffset, pmi.m_cpSize - pmi.m_cpExtractionOffset, 0);

					if (bytesReceived == 0)
					{
						closeConnection(connectionIndex, "Failed to recv: connection is closed");
						continue;
					}
					else if (bytesReceived == SOCKET_ERROR)
					{
						const int error = WSAGetLastError();
						if (error != WSAEWOULDBLOCK)
						{
							closeConnection(connectionIndex, "Socket error: " + error);
							continue;
						}
						//WSAEWOULDBLOCK �������� ��� recv �� �������� ����������� ���������, � ��� ����� ����������� ���������.
						//�� ��������� ����������, � ������ ��������� �� ��������� ��������.
						std::cerr << connection.getConnectionInfo() << "WSAEWOULDBLOCK on reading.\n";
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
								closeConnection(connectionIndex, "Packet size is too large.");
								continue;
							}
							pmi.m_cpExtractionOffset = 0;
							pmi.m_cpTask = PacketTask::PROCESS_PACKET_CONTENTS;
						}
					}
					else //process packet contents
					{
						if (pmi.m_cpExtractionOffset == pmi.m_cpSize) //���� ��������� �������� �����
						{	//�� �� ����� ����� ���������� � ����� ������ ��� ����� �����, ��� �� ����� ��������� ���� ���� ������. ���� ��� ���� ������, �� ��� �����
							std::shared_ptr<Packet> packet = std::make_shared<Packet>(); //������ ��� ��� �������� ��������� �� ���� ������ � ������� �������
							packet->m_buffer.resize(pmi.m_cpSize); //��������� ����� ����� ������������ ������ �����������. � ���� ������ ������� ����� ��������������
							memcpy(&packet->m_buffer[0], connection.m_buffer, pmi.m_cpSize); //�����, �� �� ��������� �������� �� ���������� ��� ���������

							pmi.append(packet);

							pmi.m_cpExtractionOffset = 0;
							pmi.m_cpSize = 0;
							pmi.m_cpTask = PacketTask::PROCESS_PACKET_SIZE;
						}
					}
				}

				if (currentConnectionFD.revents & POLLWRNORM) //������������� ������ ���������� � ������ ������
				{
					PacketManager& pmo = *connection.m_pmOutgoing;
					while (pmo.hasPendingPacket()) //��������� ���� �� ��� ����������. � �������� ��������� ��������� ��������� ��� ��������� ������
					{
						int bytesSended = 0;
						if (pmo.m_cpTask == PacketTask::PROCESS_PACKET_SIZE)
						{
							pmo.m_cpSize = static_cast<uint16_t>(pmo.front()->m_buffer.size());
							const uint16_t encodedPacketSize = htons(pmo.m_cpSize);

							bytesSended = send(currentConnectionFD.fd, (char*)&encodedPacketSize + pmo.m_cpExtractionOffset, sizeof(uint16_t) - pmo.m_cpExtractionOffset, 0);
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
							bytesSended = send(currentConnectionFD.fd, &pmo.front()->m_buffer[0] + pmo.m_cpExtractionOffset, pmo.m_cpSize - pmo.m_cpExtractionOffset, 0);
							if (bytesSended > 0) //��� WSAEWOULDBLOCK bytesSended ���� -1 //���� �������� 1 ����, �� ����� WSAEWOULDBLOCK? bytesSended = -1 ??
								pmo.m_cpExtractionOffset += bytesSended; //��� ��, ������ WSAE ����������� ����� ������ �� �������� ���������. ��� TCP �� ������� ���������

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
								closeConnection(connectionIndex, "Socket error: " + error);
								continue;
							}//��� send ����� �����, � non-blocking �� ���� ���������� ������� - ��������� � ��������� ������
							std::cerr << connection.getConnectionInfo() << "WSAEWOULDBLOCK on writing. Breaking.\n";
							break; //����� ����� ����� ���������� �������� �� ���������� ����� - ����������� ���������
						}
					}
					if (pmo.hasPendingPacket() == false)
						currentConnectionFD.events = POLLRDNORM;
				}
				if (currentConnectionFD.revents)
					currentConnectionFD.revents = 0;
			}
		}
		//����� ��������� ���� ������� � ������� ��������
		for (int i = static_cast<int>(m_connections.size()) - 1; i >= 0; --i) //��� ������� ����������
		{
			while (m_connections[i].m_pmIncoming->hasPendingPacket()) //������������ ��� ��������
			{
				if (!processPacket(m_connections[i], m_connections[i].m_pmIncoming->front()))
				{
					closeConnection(i, "Failed to process packet.");
					break;
				}
				m_connections[i].m_pmIncoming->popFront();
			}
		}
	}

	void Server::onConnect(TCPConnection& newConnection)
	{
		std::cout << "New connection accepted: " << newConnection.getConnectionInfo() << '\n';
	}

	bool Server::processPacket(TCPConnection& connection, std::shared_ptr<Packet> packet)
	{
		std::cout << "From " << connection.getConnectionInfo() << " received packet with size : " << packet->m_buffer.size() << '\n';

		return true;
	}

	void Server::onDisconnect(const TCPConnection& lostConnection, const std::string_view reason)
	{
		std::cerr << '[' << reason << "] Connection lost: " << lostConnection.getConnectionInfo() << ".\n";
	}

	void Server::closeConnection(const int connectionIndex, const std::string_view reason)
	{
		TCPConnection& connection = m_connections[connectionIndex];
		onDisconnect(connection, reason);

		m_master_fd.erase(m_master_fd.begin() + connectionIndex + 1);
		connection.closeTCPConnection();
		m_connections.erase(m_connections.begin() + connectionIndex);
	}
}