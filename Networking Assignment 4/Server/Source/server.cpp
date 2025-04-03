#include "server.h"

#include <iostream>
#include <cstring>

Server::Server(uint16_t port)
	: _nextPlayerNum(1), _queue(4, 100, _action, _disconnect)
{
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		std::cerr << "WSAStartup failed!" << std::endl;
		exit(EXIT_FAILURE);
	}

	_sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (_sockfd == INVALID_SOCKET) {
		std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
		WSACleanup();
		exit(EXIT_FAILURE);
	}

	memset(&_serverAddr, 0, sizeof(_serverAddr));
	_serverAddr.sin_family = AF_INET;
	_serverAddr.sin_addr.s_addr = INADDR_ANY;
	_serverAddr.sin_port = htons(port);

	if (bind(_sockfd, (SOCKADDR*)&_serverAddr, sizeof(_serverAddr)) == SOCKET_ERROR) {
		std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
		closesocket(_sockfd);
		WSACleanup();
		exit(EXIT_FAILURE);
	}

	std::cout << "Server is running on port " << port << std::endl;
}

Server::~Server() {
	closesocket(_sockfd);
	WSACleanup();
}

void Server::run() {
	while (true) {
		char buffer[1024];
		sockaddr_in clientAddr{};
		int addrLen = sizeof(clientAddr);
		int bytes = recvfrom(_sockfd, buffer, sizeof(buffer), 0, (SOCKADDR*)&clientAddr, &addrLen);

		if (bytes <= 0) continue;

		TaskItem item;
		item.data.assign(buffer, buffer + bytes);
		item.client.address = clientAddr;
		item.client.isConnected = true;

		std::string key = getClientKey(clientAddr);
		if (_clients.find(key) == _clients.end()) {
			item.client.player_num = assignPlayerNum(clientAddr);
			_clients[key] = item.client;
			std::cout << "New client connected: Player #" << item.client.player_num << std::endl;
		}
		else {
			item.client.player_num = _clients[key].player_num;
		}

		_queue.produce(item);
	}
}

uint16_t Server::assignPlayerNum(const sockaddr_in& clientAddr) {
	return _nextPlayerNum++;
}

std::string Server::getClientKey(const sockaddr_in& addr) const {
	char ip[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &(addr.sin_addr), ip, INET_ADDRSTRLEN);
	return std::string(ip) + ":" + std::to_string(ntohs(addr.sin_port));
}

bool TaskAction::operator()(const TaskItem& item) {
	CMDID cmd = static_cast<CMDID>(item.data[0]);

	switch (cmd) {
	case CMDID::REQ_QUIT:
		std::cout << "[REQ_QUIT] Player #" << item.client.player_num << " disconnected." << std::endl;
		break;
	case CMDID::CMD_TEST:
		std::cout << "[CMD_TEST] From Player #" << item.client.player_num << std::endl;
		break;
	case CMDID::SEND_PLAYERS:
		std::cout << "[SEND_PLAYERS] From Player #" << item.client.player_num << std::endl;
		break;
	default:
		std::cout << "[UNKNOWN CMD] " << (int)cmd << " from Player #" << item.client.player_num << std::endl;
		break;
	}

	return true;
}

void OnDisconnectHandler::operator()() {
	std::cout << "Server is shutting down TaskQueue." << std::endl;
}
