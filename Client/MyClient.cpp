#include "MyClient.h"
#include <fstream>
#include <thread>
#include <chrono>

void MyClient::checkVersion()
{
	std::shared_ptr<Packet> clientVersionPacket = std::make_shared<Packet>(PacketType::PACKET_CLIENT_VERSION);
	*clientVersionPacket << m_majorVersion << m_minorVersion;
	m_connection->m_pmOutgoing->append(clientVersionPacket);
	std::unique_lock<std::mutex> ul(m_mutex);
	m_conditionVariable.wait_for(ul, std::chrono::milliseconds(500), [&]() { return m_resumeThread; }); //ждем ответ-сообщения сервера перед возвратом в меню
	m_resumeThread = false;
}

void MyClient::onConnect(std::shared_ptr<Client> client)
{
	std::cout << "Connection established: " << '[' << m_connectionInfo << "]\n";

	MyClient* myClient = dynamic_cast<MyClient*>(client.get());
	if (myClient)
	{
		std::thread clientInput(&MyClient::runMenu, myClient);
		clientInput.detach();
	}
	else
		std::cerr << __func__ << " - Failed to cast Client* to MyClient*\n";
}

void MyClient::runMenu()
{
	while (1)
	{
		std::cout << "To enter chat input 1\n";
		std::cout << "To send file input 2\n";
		std::cout << "To check version input 0\n";

		std::string input;
		std::getline(std::cin, input);
		if (!(input.length() == 1 && isdigit(input[0])))
			continue;

		int digit = std::stoi(input);
		switch (digit)
		{
		case 1: runChat(); break;
		case 2: runFileSender(); break;
		case 0: checkVersion(); break;
		}
	}
}

void MyClient::runChat()
{
	std::cout << "You are currently in [chat] mode. To return to the [menu] input #\n";
	while (1)
	{
		std::string message = "";
		std::getline(std::cin, message);
		if (message == "#")
		{
			std::cout << '\n';
			break;
		}

		std::shared_ptr<Packet> clientMessagePacket = std::make_shared<Packet>(PacketType::PACKET_CLIENT_MESSAGE);
		*clientMessagePacket << message;
		m_connection->m_pmOutgoing->append(clientMessagePacket);
		std::cout << "Sended: " << message << '\n';
	}
	return;
}

std::filesystem::path MyClient::inputFilePath()
{
	std::cout << "You are currently in [file sending] mode. To return to the [menu] input #\n";

	std::cout << "Enter a full path to the file in the format C:\\dir1\\file.txt\n";
	std::string path = "";
	bool pathExists = false;
	do
	{
		std::getline(std::cin, path);
		if (path == "#")
		{
			std::cout << '\n';
			return path;
		}

		std::filesystem::path filePath(path);
		if (std::filesystem::exists(filePath))
		{
			if (std::filesystem::file_size(filePath) == 0)
			{
				std::cerr << "File is 0 bytes size. Sending is not possible.\n";
				continue;
			}
			std::cout << "Path is exists!\n";
			pathExists = true;
		}
		else
			std::cout << "Can't find that path.\n";
	} while (pathExists == false);

	return path;
}

bool MyClient::runFileSender()
{
	m_fileInfo.filePath = inputFilePath();
	if (m_fileInfo.filePath == "#")
		return false;
	m_fileInfo.fileName = m_fileInfo.filePath.filename();

	//Отправляем количество пакетов, на которые будет разбит файл, и название файла:
	std::ifstream fileInfo(m_fileInfo.filePath.c_str(), std::ios::ate);
	if (!fileInfo)
	{
		std::cerr << "Can't open the file " << m_fileInfo.fileName << "\n\n";
		return false;
	}
	m_fileInfo.fileSize = static_cast<uint64_t>(fileInfo.tellg());
	if (m_fileInfo.fileSize > PNet::g_MaxFileSize)
	{
		std::cerr << "Maximum file size exceeded.\n\n";
		return false;
	}
	if (m_fileInfo.fileSize < 1048576) //1megabyte
		std::cout << "Sending file of " << m_fileInfo.fileSize << " bytes...\n";
	else if (m_fileInfo.fileSize < 1073741824) //1gigabyte
		std::cout << "Sending file of " << m_fileInfo.fileSize / 1024 << " kilobytes...\n";
	else 
		std::cout << "Sending file of " << m_fileInfo.fileSize / 1048576 << " megabytes...\n";
	fileInfo.close();

	if (m_fileInfo.fileSize <= PNet::g_MaxChunkSize && m_fileInfo.fileSize > 0)
		m_fileInfo.chunksCount = 1;
	else
		m_fileInfo.chunksCount = static_cast<int>(std::ceil(static_cast<double>(m_fileInfo.fileSize) / PNet::g_MaxChunkSize));

	std::shared_ptr<Packet> fileInfoPacket = std::make_shared<Packet>(PacketType::PACKET_FILE_INFO);
	*fileInfoPacket << m_fileInfo.fileName;
	*fileInfoPacket << m_fileInfo.chunksCount;
	m_connection->m_pmOutgoing->append(fileInfoPacket);

	std::unique_lock<std::mutex> ul(m_mutex);
	m_conditionVariable.wait_for(ul, std::chrono::milliseconds(1000), [&]() { return m_resumeThread; }); //ждем ответ-сообщения сервера с разрешением на отправку тела файла
	m_resumeThread = false;
	if (m_send_permission == false)
	{
		std::cout << "The file was not sent.\n\n";
		return false;
	}

	sendFileBody();
	return true;
}

bool MyClient::sendFileBody()
{
	std::ifstream inf(m_fileInfo.filePath.c_str(), std::ios::in | std::ios::binary);
	if (!inf)
	{
		std::cerr << "Can't open the file " << m_fileInfo.fileName << "\n";
		return false;
	}

	std::string buffer;
	for (uint32_t chunk = 1; chunk <= m_fileInfo.chunksCount; ++chunk)
	{
		if (chunk == m_fileInfo.chunksCount)
		{
			const uint16_t readCount = m_fileInfo.fileSize % PNet::g_MaxChunkSize;
			buffer.resize(readCount);
			inf.read(&buffer[0], readCount);
		}
		else
		{
			buffer.resize(PNet::g_MaxChunkSize);
			inf.read(&buffer[0], PNet::g_MaxChunkSize);
		}

		std::shared_ptr<Packet> fileChunkPacket = std::make_shared<Packet>(PacketType::PACKET_FILE_CHUNK);
		*fileChunkPacket << chunk << buffer;
		m_connection->m_pmOutgoing->append(fileChunkPacket);
		//std::cout << "Outgoing " << chunk << '/' << m_fileInfo.chunksCount << " chunk. Size: " << buffer.length() << " bytes." << "\n";
		if (chunk == m_fileInfo.chunksCount)
			std::cout << "File sended.\n\n";
	}
	inf.close();
	return true;
}

bool MyClient::processPacket(Packet& packet)
{
	switch (packet.getPacketType())
	{
	case PacketType::PACKET_UPLOAD_PERMISSION:
	{
		packet >> m_send_permission;
		m_resumeThread = true;
		if (m_send_permission) //разрешение на отправку файла + клиентам придет CLIENT_IDENTIFIER, за ним FILE_INFO
			m_conditionVariable.notify_one();
		//иначе придет пакет SERVER_MESSAGE
		break;
	}
	case PacketType::PACKET_CLIENT_VERSION:
	{
		m_resumeThread = true;
		[[fallthrough]];
	}
	case PacketType::PACKET_SERVER_MESSAGE:
	{
		std::string server_message;
		packet >> server_message;

		std::unique_lock<std::mutex> ul(m_mutex);
		std::cout << "Server: " << server_message << "\n\n";

		if (m_resumeThread)
			m_conditionVariable.notify_one();

		break;
	}
	case PacketType::PACKET_CLIENT_IDENTIFIER:
	{
		std::string clientIdentifier;
		packet >> clientIdentifier;
		std::unique_lock<std::mutex> ul(m_mutex);
		std::cout << clientIdentifier << ": ";
		break;
	}
	case PacketType::PACKET_CLIENT_MESSAGE:
	{
		std::string client_message;
		packet >> client_message;
		std::unique_lock<std::mutex> ul(m_mutex);
		std::cout << client_message << '\n';
		break;
	}
	case PacketType::PACKET_FILE_INFO:
	{
		packet >> m_fileInfo.fileName;
		packet >> m_fileInfo.chunksCount; //no disk space?
		std::string senderInfo;
		packet >> senderInfo;
		std::unique_lock<std::mutex> ul(m_mutex);
		std::cout << "Server: Receiving file " << m_fileInfo.fileName << " from " << senderInfo << "...\n";
		break;
	}
	case PacketType::PACKET_FILE_CHUNK:
	{
		uint32_t chunk;
		packet >> chunk;

		if (chunk == 1)
		{
			std::ofstream fileDelete(m_fileInfo.fileName.string(), std::ios::trunc);//
			fileDelete.close();
		}

		std::ofstream outf(m_fileInfo.fileName.string(), std::ios::out | std::ios::binary | std::ios::app);//
		if (!outf)
		{
			std::cerr << "Cant't open a file\n";
			break;
		}

		std::string buffer;
		buffer.reserve(PNet::g_MaxChunkSize);
		packet >> buffer;

		outf << buffer;
		outf.close();

		//std::cout << "Received " << chunk << '/' << m_fileInfo.chunksCount << " chunks of data.\n";

		if (chunk == m_fileInfo.chunksCount)
		{
			m_fileInfo.filePath = std::filesystem::current_path();
			m_fileInfo.filePath.append(m_fileInfo.fileName.string());
			std::unique_lock<std::mutex> ul(m_mutex);
			std::cout << "File received and located on the path: " << m_fileInfo.filePath << '\n';
			m_fileInfo.chunksCount = 0;
		}

		break;
	}
	case PacketType::PACKET_INTEGER_ARRAY:
	{
		uint32_t arraySize;
		packet >> arraySize;
		std::unique_lock<std::mutex> ul(m_mutex);
		std::cout << "Array size: " << arraySize << '\n';
		for (uint32_t i = 0; i < arraySize; ++i)
		{
			uint32_t el;
			packet >> el;
			std::cout << "Element [ " << i << " ] - " << el << '\n';
		}
		break;
	}
	default:
		std::unique_lock<std::mutex> ul(m_mutex);
		std::cerr << "Unrecognized packet type.\n";
		return false;
	}

	return true;
}