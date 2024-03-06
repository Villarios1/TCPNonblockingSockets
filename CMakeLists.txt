cmake_minimum_required(VERSION 3.1)
set(CMAKE_CXX_STANDARD 20)
set(CXX_STANDARD_REQUIRED TRUE)
project (PNet VERSION 1.0 LANGUAGES CXX)
include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
add_executable(Server Server/Source.cpp
    Server/MyServer.h Server/MyServer.cpp
    PNet/PNet.h
    PNet/Network.h PNet/Network.cpp
    PNet/Server.h PNet/Server.cpp 
    PNet/Client.h PNet/Client.cpp
    PNet/Constants.h
    PNet/IPEndpoint.h PNet/IPEndpoint.cpp
    PNet/IPVersion.h
    PNet/Packet.h PNet/Packet.cpp
    PNet/PacketException.h
    PNet/PacketManager.h PNet/PacketManager.cpp
    PNet/PacketType.h
    PNet/Socket.h PNet/Socket.cpp
    PNet/SocketOption.h
    PNet/TCPConnection.h PNet/TCPConnection.cpp)
add_executable(Client Client/Source.cpp
    Client/MyClient.h Client/MyClient.cpp
    PNet/PNet.h
    PNet/Network.h PNet/Network.cpp
    PNet/Server.h PNet/Server.cpp 
    PNet/Client.h PNet/Client.cpp
    PNet/Constants.h
    PNet/IPEndpoint.h PNet/IPEndpoint.cpp
    PNet/IPVersion.h
    PNet/Packet.h PNet/Packet.cpp
    PNet/PacketException.h
    PNet/PacketManager.h PNet/PacketManager.cpp
    PNet/PacketType.h
    PNet/Socket.h PNet/Socket.cpp
    PNet/SocketOption.h
    PNet/TCPConnection.h PNet/TCPConnection.cpp)
set_target_properties(Server PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_target_properties(Client PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")