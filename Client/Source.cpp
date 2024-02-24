#include "MyClient.h"
#include <Windows.h>

int main()
{
	SetConsoleCP(1251);
	SetConsoleOutputCP(1251);

	if (Network::initialize() == false)
	{
		std::cerr << "Winsock api failed to initialize.\n";
		return 0;
	}
	std::cout << "Winsock api successfully initialized.\n";

	std::shared_ptr<MyClient> client = std::make_shared<MyClient>();
	if (client->connect(std::make_unique<IPEndpoint>("HOME-PC", 6112), client))
	{
		while (client->isConnected())
		{
			client->frame();
		}
	}

	Network::shutdown();
	system("pause");

	return 0;
}