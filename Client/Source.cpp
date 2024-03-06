#include "MyClient.h"
#include <stdlib.h>

int main()
{
	#ifdef _WIN32
	SetConsoleCP(1251);
	SetConsoleOutputCP(1251);

	if (Network::initialize() == false)
	{
		std::cerr << "Winsock api failed to initialize.\n";
		return 0;
	}
	std::cout << "Winsock api successfully initialized.\n";
	#endif

	std::shared_ptr<MyClient> client = std::make_shared<MyClient>();
	if (client->connect(std::make_unique<IPEndpoint>("::1", 6112), client))
	{
		while (client->isConnected())
		{
			client->frame();
		}
	}

	#ifdef _WIN32
	Network::shutdown();
	system("pause");
	#endif

	return 0;
}