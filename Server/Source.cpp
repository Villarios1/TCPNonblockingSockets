#include "MyServer.h"
#include <Windows.h>

int main()
{
	SetConsoleCP(1251);
	SetConsoleOutputCP(1251);

	if (Network::initialize() == false)
	{
		std::cout << "Winsock api failed to initialize.\n";
		return 0;
	}
	std::cout << "Winsock api successfully initialized.\n";

	std::unique_ptr<MyServer> server = std::make_unique<MyServer>();
	if (server->initialize(std::make_unique<IPEndpoint>("HOME-PC", 6112)))
	{
		while (1)
		{
			server->frame();
		}
	}

	Network::shutdown();
	system("pause");

	return 0;
}