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
			m_listeningSocket.close();
			return false;
		}
		std::cout << "Socket listening a " << ipEndpoint->getPort() << " port\n\n";

		pollfd listeningSocketFD;
		listeningSocketFD.fd = m_listeningSocket.getHandle();
		listeningSocketFD.events = POLLRDNORM;
		listeningSocketFD.revents = 0;
		m_master_fd.push_back(listeningSocketFD); //первый экземпляр - сокет сервера (слушающий)

		return true;
	}

	void Server::frame()
	{
		for (int i = 0; i < static_cast<int>(m_connections.size()); ++i)
			if (m_connections[i].m_pmOutgoing->hasPendingPacket()) //если в соединении есть очередь на отправку данных
				m_master_fd[i + 1].events = POLLRDNORM | POLLWRNORM; //устанавливаем ему что помимо приема в этой итерации нужно проверить еще и отправку

		const int result = WSAPoll(m_master_fd.data(), static_cast<ULONG>(m_master_fd.size()), 50);
		if (result == SOCKET_ERROR)
		{
			m_listeningSocket.printErrorDescription(__func__, WSAGetLastError());
			m_listeningSocket.close();
			return;
		}
		if (result > 0) //если случился ивент:
		{
#pragma region listener
			pollfd& listeningSocketFD = m_master_fd[0];
			if (listeningSocketFD.revents & POLLRDNORM) //если кто-то пытается connect к слушающему сокету - у него есть что считать - принимаем подключение
			{
				listeningSocketFD.revents = 0;

				Socket newConnectionSocket; //сокет на сервере, отвечающий за связь с новым клиентом
				std::unique_ptr<IPEndpoint> newConnectionEndpoint = std::make_unique<IPEndpoint>();
				if (!m_listeningSocket.acceptSocket(newConnectionSocket, newConnectionEndpoint.get()))
				{
					m_listeningSocket.close();
					return;
				}

				pollfd newConnectionFD = {};
				newConnectionFD.fd = newConnectionSocket.getHandle();
				newConnectionFD.events = POLLRDNORM; //устанавливаем в новом сокете проверку на входные данные
				newConnectionFD.revents = 0;
				m_master_fd.push_back(newConnectionFD);
				m_connections.emplace_back(newConnectionSocket, std::move(newConnectionEndpoint));

				TCPConnection& acceptedConnection = m_connections[m_connections.size() - 1];
				onConnect(acceptedConnection);
			}
#pragma endregion Code spesific to the listening socket
			for (int i = static_cast<int>(m_master_fd.size()) - 1; i >= 1; --i) //и проверяем события в каждом сокете для подключившихся:
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
				if (currentConnectionFD.revents & POLLRDNORM) //если в сокет был send - есть что считать - получаем данные
				{
					PacketManager& pmi = *connection.m_pmIncoming; //на текущем соединении (в сервере) записываем данные в очередь на прием

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
						//WSAEWOULDBLOCK означает что recv не успевает выполниться мгновенно, и это будет блокирующей операцией.
						//Не закрываем соединение, а просто продолжим на следующей итерации.
						std::cerr << connection.getConnectionInfo() << "WSAEWOULDBLOCK on reading.\n";
					}

					if (bytesReceived > 0) //при WSAEWOULDBLOCK bytesReceived тоже -1 //если отправит 1 байт, но будет WSAEWOULDBLOCK? bytesReceived = -1 ??
						pmi.m_cpExtractionOffset += bytesReceived;

					if (pmi.m_cpTask == PacketTask::PROCESS_PACKET_SIZE)
					{
						if (pmi.m_cpExtractionOffset == sizeof(pmi.m_cpSize)) //если полностью получили размер пакета
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
						if (pmi.m_cpExtractionOffset == pmi.m_cpSize) //если полностью получили пакет
						{	//мы не можем сразу записывать в пакет потому что нужно место, где он будет храниться пока идет запись. Если это член класса, то это буфер
							std::shared_ptr<Packet> packet = std::make_shared<Packet>(); //потому что при отправке указателя на член класса в очередь пакетов
							packet->m_buffer.resize(pmi.m_cpSize); //следующий пакет будет переписывать память предыдущего. А если внутри очереди будет недозаписанный
							memcpy(&packet->m_buffer[0], connection.m_buffer, pmi.m_cpSize); //пакет, то на ближайшей проверке он попытается его отправить

							pmi.append(packet);

							pmi.m_cpExtractionOffset = 0;
							pmi.m_cpSize = 0;
							pmi.m_cpTask = PacketTask::PROCESS_PACKET_SIZE;
						}
					}
				}

				if (currentConnectionFD.revents & POLLWRNORM) //необходимость записи помечается в начале фрейма
				{
					PacketManager& pmo = *connection.m_pmOutgoing;
					while (pmo.hasPendingPacket()) //Проверяем есть ли что отправлять. И пытаемся мгновенно отправить попакетно все ожидающие пакеты
					{
						int bytesSended = 0;
						if (pmo.m_cpTask == PacketTask::PROCESS_PACKET_SIZE)
						{
							pmo.m_cpSize = static_cast<uint16_t>(pmo.front()->m_buffer.size());
							const uint16_t encodedPacketSize = htons(pmo.m_cpSize);

							bytesSended = send(currentConnectionFD.fd, (char*)&encodedPacketSize + pmo.m_cpExtractionOffset, sizeof(uint16_t) - pmo.m_cpExtractionOffset, 0);
							if (bytesSended > 0) //при WSAEWOULDBLOCK bytesSended тоже -1 //если отправит 1 байт, но будет WSAEWOULDBLOCK? bytesSended = -1 ??
								pmo.m_cpExtractionOffset += bytesSended;

							if (pmo.m_cpExtractionOffset == sizeof(uint16_t)) //если размер полностью передан
							{
								pmo.m_cpExtractionOffset = 0;
								pmo.m_cpTask = PacketTask::PROCESS_PACKET_CONTENTS;
							}
						}
						else //process packet contents
						{
							bytesSended = send(currentConnectionFD.fd, &pmo.front()->m_buffer[0] + pmo.m_cpExtractionOffset, pmo.m_cpSize - pmo.m_cpExtractionOffset, 0);
							if (bytesSended > 0) //при WSAEWOULDBLOCK bytesSended тоже -1 //если отправит 1 байт, но будет WSAEWOULDBLOCK? bytesSended = -1 ??
								pmo.m_cpExtractionOffset += bytesSended; //все ок, видимо WSAE выскакивает когда вообще не успевает отправить. Или TCP не пускает недопакет

							if (pmo.m_cpExtractionOffset == pmo.m_cpSize) //если пакет полностью передан
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
							}//для send нужно время, а non-blocking не ждет завершения функции - продолжим в следующем фрейме
							std::cerr << connection.getConnectionInfo() << "WSAEWOULDBLOCK on writing. Breaking.\n";
							break; //иначе будем ждать завершения отправки во внутреннем цикле - блокирующее поведение
						}
					}
					if (pmo.hasPendingPacket() == false)
						currentConnectionFD.events = POLLRDNORM;
				}
				if (currentConnectionFD.revents)
					currentConnectionFD.revents = 0;
			}
		}
		//после обработки всех событий в текущей итерации
		for (int i = static_cast<int>(m_connections.size()) - 1; i >= 0; --i) //для каждого соединения
		{
			while (m_connections[i].m_pmIncoming->hasPendingPacket()) //обрабатываем все входящие
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