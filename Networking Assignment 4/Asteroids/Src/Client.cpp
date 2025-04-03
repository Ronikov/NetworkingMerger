#include "Client.h"

#include <iostream>
#include <string>
#include <winsock2.h>
#include <chrono>
#include <thread>

#include "AtomicVariables.h"

#pragma comment(lib, "Ws2_32.lib")

constexpr uint16_t SERVER_PORT = 9000;
constexpr int TIMEOUT_MS = 1000;
constexpr size_t BUFFER_SIZE = 1024;

SOCKET broadcast_socket;
sockaddr_in serverAddr;
int serverAddrSize = sizeof(serverAddr);

bool InitClient()
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "WSAStartup failed\n";
        return false;
    }

    broadcast_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (broadcast_socket == INVALID_SOCKET)
    {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << '\n';
        WSACleanup();
        return false;
    }

    return true;
}

bool ConnectToServer()
{
    // Enable broadcast
    BOOL bOptVal = TRUE;
    if (setsockopt(broadcast_socket, SOL_SOCKET, SO_BROADCAST, (char*)&bOptVal, sizeof(bOptVal)) == SOCKET_ERROR)
    {
        std::cerr << "Broadcast option failed: " << WSAGetLastError() << '\n';
        closesocket(broadcast_socket);
        WSACleanup();
        return false;
    }

    // Set receive timeout
    DWORD timeout = TIMEOUT_MS;
    if (setsockopt(broadcast_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)) == SOCKET_ERROR)
    {
        std::cerr << "Timeout option failed: " << WSAGetLastError() << '\n';
        closesocket(broadcast_socket);
        WSACleanup();
        return false;
    }

    sockaddr_in broadcastAddr{};
    broadcastAddr.sin_family = AF_INET;
    broadcastAddr.sin_port = htons(SERVER_PORT);
    broadcastAddr.sin_addr.s_addr = INADDR_BROADCAST;

    while (!av_connected && !av_start)
    {
        const char* broadcastMessage = "Hello, server!";
        if (sendto(broadcast_socket, broadcastMessage, strlen(broadcastMessage), 0, (SOCKADDR*)&broadcastAddr, sizeof(broadcastAddr)) == SOCKET_ERROR)
        {
            std::cerr << "Broadcast failed: " << WSAGetLastError() << '\n';
            closesocket(broadcast_socket);
            WSACleanup();
            return false;
        }

        std::cout << "Broadcast sent...\n";

        char recvBuffer[BUFFER_SIZE];
        int bytesReceived = recvfrom(broadcast_socket, recvBuffer, sizeof(recvBuffer), 0, (SOCKADDR*)&serverAddr, &serverAddrSize);

        while (bytesReceived == SOCKET_ERROR)
        {
            int err = WSAGetLastError();
            if (err == WSAETIMEDOUT)
            {
                std::cout << "No response yet, retrying...\n";
                bytesReceived = recvfrom(broadcast_socket, recvBuffer, sizeof(recvBuffer), 0, (SOCKADDR*)&serverAddr, &serverAddrSize);
                continue;
            }

            std::cerr << "recvfrom failed: " << err << '\n';
            closesocket(broadcast_socket);
            WSACleanup();
            return false;
        }

        recvBuffer[bytesReceived] = '\0';
        int num = std::stoi(recvBuffer);

        if (num >= 1 && num <= 4) {
            av_connected = true;
            av_player_num = num;
            std::cout << "Connected to server as Player " << av_player_num << '\n';
        }
        else {
            std::cout << "Received game start signal\n";
            av_start = true;
        }
    }

    return true;
}

bool WaitForStart()
{
    DWORD timeout = TIMEOUT_MS;
    if (setsockopt(broadcast_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)) == SOCKET_ERROR)
    {
        std::cerr << "Timeout option failed: " << WSAGetLastError() << '\n';
        closesocket(broadcast_socket);
        WSACleanup();
        return false;
    }

    char recvBuffer[BUFFER_SIZE];
    int bytesReceived = recvfrom(broadcast_socket, recvBuffer, sizeof(recvBuffer), 0, (SOCKADDR*)&serverAddr, &serverAddrSize);

    if (bytesReceived == SOCKET_ERROR)
    {
        int err = WSAGetLastError();
        if (err == WSAETIMEDOUT)
        {
            std::cout << "Waiting for game start signal...\n";
            return false;
        }

        std::cerr << "recvfrom failed: " << err << '\n';
        return false;
    }

    recvBuffer[bytesReceived] = '\0';
    av_player_max = std::stoi(recvBuffer);

    std::cout << "Game start signal received. Max Players: " << av_player_max << '\n';
    av_start = true;
    return true;
}

void ShutDownClient()
{
    closesocket(broadcast_socket);
    WSACleanup();
}

SOCKET GetSocket()
{
    return broadcast_socket;
}
