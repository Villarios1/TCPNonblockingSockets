#include "MyServer.h"
#include <stdlib.h>

int main()
{
	#ifdef _WIN32
	SetConsoleCP(1251);
	SetConsoleOutputCP(1251);

	if (Network::initialize() == false)
	{
		std::cout << "Winsock api failed to initialize.\n";
		return 0;
	}
	#endif
	std::cout << "Winsock api successfully initialized.\n";

	std::unique_ptr<MyServer> server = std::make_unique<MyServer>();
	if (server->initialize(std::make_unique<IPEndpoint>("::1", 6112)))
	{
		while (1)
		{
			server->frame();
		}
	}

	#ifdef _WIN32
	Network::shutdown();
	system("pause");
	#endif
	
	return 0;
}