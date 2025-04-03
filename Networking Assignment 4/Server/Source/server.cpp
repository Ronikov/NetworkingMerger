#include "server.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <cstring>

#pragma comment(lib, "ws2_32.lib")

const int MAX_PLAYERS = 4;
SOCKET udp_socket;
std::vector<ClientInfo> clients(MAX_PLAYERS);
std::vector<Player> players(MAX_PLAYERS);

void InitNetwork() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(9000);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    bind(udp_socket, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
    std::cout << "[Server] Listening on port 9000...\n";
}

void Cleanup() {
    closesocket(udp_socket);
    WSACleanup();
}

void BroadcastToAll(const char* message) {
    for (auto& client : clients) {
        if (client.isConnected) {
            sendto(udp_socket, message, strlen(message), 0,
                (SOCKADDR*)&client.address, sizeof(client.address));
        }
    }
}

void SyncAllPlayers() {
    CMDID cmd = SEND_PLAYERS;
    size_t totalSize = sizeof(cmd) + MAX_PLAYERS * sizeof(Player);
    std::vector<char> buffer(totalSize);

    memcpy(buffer.data(), &cmd, sizeof(cmd));
    memcpy(buffer.data() + sizeof(cmd), players.data(), MAX_PLAYERS * sizeof(Player));

    for (auto& client : clients) {
        if (client.isConnected) {
            sendto(udp_socket, buffer.data(), totalSize, 0,
                (SOCKADDR*)&client.address, sizeof(client.address));
        }
    }
}

void ReceiveClients() {
    sockaddr_in senderAddr;
    int addrSize = sizeof(senderAddr);
    char recvBuffer[1024];

    int bytes = recvfrom(udp_socket, recvBuffer, sizeof(recvBuffer), 0,
        (SOCKADDR*)&senderAddr, &addrSize);

    if (bytes == SOCKET_ERROR) return;

    // Check if new client
    bool found = false;
    for (auto& client : clients) {
        if (client.isConnected &&
            client.address.sin_addr.S_un.S_addr == senderAddr.sin_addr.S_un.S_addr &&
            client.address.sin_port == senderAddr.sin_port) {
            found = true;
            break;
        }
    }

    if (!found) {
        for (int i = 0; i < MAX_PLAYERS; ++i) {
            if (!clients[i].isConnected) {
                clients[i].address = senderAddr;
                clients[i].isConnected = true;
                clients[i].player_num = i + 1;
                std::string pnum = std::to_string(i + 1);
                sendto(udp_socket, pnum.c_str(), pnum.size(), 0, (SOCKADDR*)&senderAddr, sizeof(senderAddr));
                std::cout << "[Client] Player " << (i + 1) << " connected.\n";
                break;
            }
        }
    }
    else {
        // Handle gameplay data
        CMDID cmd;
        memcpy(&cmd, recvBuffer, sizeof(cmd));

        for (auto& client : clients) {
            if (client.isConnected &&
                client.address.sin_addr.S_un.S_addr == senderAddr.sin_addr.S_un.S_addr &&
                client.address.sin_port == senderAddr.sin_port) {

                if (cmd == SEND_PLAYERS) {
                    Player playerData;
                    memcpy(&playerData, recvBuffer + sizeof(cmd), sizeof(Player));
                    players[client.player_num - 1] = playerData;

                    char ip[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &(senderAddr.sin_addr), ip, INET_ADDRSTRLEN);
                    std::cout << "[Update] Player " << client.player_num << " data from "
                        << ip << ":" << ntohs(senderAddr.sin_port) << "\n";
                }
                break;
            }
        }
    }
}

void RunServer() {
    int connected = 0;
    bool started = false;
    while (true) {
        ReceiveClients();

        // Check start
        if (!started) {
            connected = 0;
            for (auto& c : clients) if (c.isConnected) ++connected;
            if (connected >= MAX_PLAYERS) {
                BroadcastToAll("4");
                started = true;
                std::cout << "[Game] Max players reached. Starting game...\n";
            }
        }
        else {
            SyncAllPlayers();
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    }
}

int main() {
    InitNetwork();
    RunServer();
    Cleanup();
    return 0;
}