#pragma once
#include "PNet.h"
#include "FileInfo.h"
#include <mutex>
#include <condition_variable>

using namespace PNet;

class MyClient : public Client
{
	uint32_t m_majorVersion = 1;
	uint32_t m_minorVersion = 0;
	void checkVersion();

	//void onConnectFail() const override;
	void onConnect(std::shared_ptr<Client> client = nullptr) override;
	//void onDisconnect(const std::string_view reason) override;
	bool processPacket(Packet& packet) override;

	std::mutex m_mutex;
	std::condition_variable m_conditionVariable;
	bool m_resumeThread = false;
	void runMenu();
	void runChat();
	bool runFileSender();

	bool m_send_permission = false;
	bool sendFileBody();
	std::filesystem::path inputFilePath();
	FileInfo m_fileInfo;
};