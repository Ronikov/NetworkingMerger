#ifndef SERVER_H
#define SERVER_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <unordered_map>
#include <vector>
#include <string>
#include <atomic>

#pragma comment(lib, "ws2_32.lib")

#include "taskqueue.h"

struct ClientInfo {
	sockaddr_in address;
	bool isConnected;
	uint16_t player_num;
};

enum CMDID : unsigned char {
	UNKNOWN = 0x0,
	REQ_QUIT = 0x1,
	REQ_LISTUSERS = 0x2,
	RSP_LISTUSERS = 0x3,
	SEND_PLAYERS = 0x4,
	RECEIVE_PLAYERS = 0x5,
	SEND_ASTEROIDS = 0x6,
	RECEIVE_ASTEROIDS = 0x7,
	SEND_BULLETS = 0x8,
	RECEIVE_BULLETS = 0x9,
	CMD_TEST = 0x20,
	ECHO_ERROR = 0x30
};

struct TaskItem {
	std::vector<char> data;
	ClientInfo client;
};

struct TaskAction {
	bool operator()(const TaskItem& item);
};

struct OnDisconnectHandler {
	void operator()();
};

class Server {
public:
	Server(uint16_t port);
	~Server();

	void run();

private:
	SOCKET _sockfd;
	sockaddr_in _serverAddr;

	std::atomic<uint16_t> _nextPlayerNum;
	std::unordered_map<std::string, ClientInfo> _clients;

	TaskAction _action;
	OnDisconnectHandler _disconnect;
	TaskQueue<TaskItem, TaskAction, OnDisconnectHandler> _queue;

	uint16_t assignPlayerNum(const sockaddr_in& clientAddr);
	std::string getClientKey(const sockaddr_in& addr) const;
};

#endif // SERVER_H
