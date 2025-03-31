/******************************************************************************/
/*!
\file		Client.cpp
\author     goh.a@digipen.edu

\date   	March 27 2025
\brief		Functions for client

Copyright (C) 2023 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
 /******************************************************************************/
#include "Client.h"

#include <iostream>
#include <string>
#include <winsock2.h>

#include "AtomicVariables.h"

#pragma comment(lib, "Ws2_32.lib")


SOCKET broadcast_socket;

bool InitClient()
{
    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "WSAStartup failed\n";
        return false;
    }

    // Create a socket for sending
    broadcast_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (broadcast_socket == INVALID_SOCKET)
    {
        std::cerr << "socket failed with error: " << WSAGetLastError() << '\n';
        WSACleanup();
        return false;
    }

    return true;
}

bool ConnectToServer()
{// Enable broadcast on the socket
    BOOL bOptVal = TRUE;
    if (setsockopt(broadcast_socket, SOL_SOCKET, SO_BROADCAST, (char*)&bOptVal, sizeof(bOptVal)) == SOCKET_ERROR)
    {
        std::cerr << "setsockopt failed with error: " << WSAGetLastError() << '\n';
        closesocket(broadcast_socket);
        WSACleanup();
        return false;
    }

    // Define the timeout for each attempt in milliseconds
    const int timeout_ms = 1000;

    // Set the receive timeout option for the socket
    DWORD timeout = timeout_ms;
    if (setsockopt(broadcast_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)) == SOCKET_ERROR)
    {
        std::cerr << "setsockopt failed with error: " << WSAGetLastError() << '\n';
        closesocket(broadcast_socket);
        WSACleanup();
        return false;
    }

    // Setup the broadcast address
    sockaddr_in broadcastAddr;
    broadcastAddr.sin_family = AF_INET;
    broadcastAddr.sin_port = htons(9000); // 9000 for Server port auto connect
    broadcastAddr.sin_addr.s_addr = INADDR_BROADCAST;

    while (!av_connected && !av_start)
    {
        // Send the broadcast message
        const char* broadcastMessage = "Hello, server!";
        if (sendto(broadcast_socket, broadcastMessage, strlen(broadcastMessage), 0, (SOCKADDR*)&broadcastAddr, sizeof(broadcastAddr)) == SOCKET_ERROR)
        {
            std::cerr << "sendto failed with error: " << WSAGetLastError() << '\n';
            closesocket(broadcast_socket);
            WSACleanup();
            return false;
        }

        std::cout << "Broadcast message sent\n";

        // Receive the response from the server
        sockaddr_in serverAddr;
        int serverAddrSize = sizeof(serverAddr);
        char recvBuffer[1024];
        int bytesReceived = recvfrom(broadcast_socket, recvBuffer, sizeof(recvBuffer), 0, (SOCKADDR*)&serverAddr, &serverAddrSize);

    	if(bytesReceived != SOCKET_ERROR && bytesReceived != SO_RCVTIMEO)
        {
            av_connected = true;
		    recvBuffer[bytesReceived] = '\0';
		    std::cout << "Received response from server: " << recvBuffer << '\n';

			av_player_num = std::stoi(recvBuffer);
        }
    }

    return true;
}

bool WaitForStart()
{
    // Define the timeout for each attempt in milliseconds
    const int timeout_ms = 1000;

    // Set the receive timeout option for the socket
    DWORD timeout = timeout_ms;
    if (setsockopt(broadcast_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)) == SOCKET_ERROR)
    {
        std::cerr << "setsockopt failed with error: " << WSAGetLastError() << '\n';
        closesocket(broadcast_socket);
        WSACleanup();
        return false;
    }

    // Receive the response from the server
    sockaddr_in serverAddr;
    int serverAddrSize = sizeof(serverAddr);
    char recvBuffer[1024];
    int bytesReceived = recvfrom(broadcast_socket, recvBuffer, sizeof(recvBuffer), 0, (SOCKADDR*)&serverAddr, &serverAddrSize);
    if (bytesReceived == SOCKET_ERROR)
    {
        std::cout << "no\n";
        return false;
    }

    recvBuffer[bytesReceived] = '\0';
    std::cout << "Received response from server: " << recvBuffer << '\n';
    av_player_max = std::stoi(recvBuffer);

    return true;
}


void ShutDownClient()
{
    // Cleanup
    closesocket(broadcast_socket);
    WSACleanup();
}

SOCKET GetSocket()
{
    return broadcast_socket;
}
