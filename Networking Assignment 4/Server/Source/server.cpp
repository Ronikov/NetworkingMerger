/******************************************************************************/
/*!
\file		GameStateManager.h
\author     goh.a@digipen.edu

\date   	March 27 2025
\brief		GameState Manager function

Copyright (C) 2023 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
 /******************************************************************************/

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "Windows.h"
#include "ws2tcpip.h"
#include <iphlpapi.h>

#pragma comment(lib, "IPHLPAPI.lib")
#pragma comment(lib, "ws2_32.lib")

#include <chrono>
#include <iostream>	
#include <string>
#include <thread>
#include <vector>

#include <map>
#include <filesystem>
#include <fstream>

#include "taskqueue.h"
#include "server.h"

//=====================================================================================
//FOR AESTEROIDS
const int			ASTEROID_SCORE = 300;			// score per asteroid destroyed
const float			ASTEROID_SIZE = 70.0f;		// asteroid size
const float			ASTEROID_SPEED = 100.0f;		// maximum asteroid speed
const float			ASTEROID_TIME = 2.0f;			// 2 second spawn time for asteroids
const int			ASTEROID_MAX = 50;			// 2 second spawn time for asteroids

const int			WINDOW_WIDTH = 800;
const int			WINDOW_HEIGHT = 600;

struct ASTEROID
{
	int		_id;
	bool	_active;

	float	_size;
	Vec2	_pos;
	Vec2	_vel;
	float	_rot;

	ASTEROID() = default;
	ASTEROID(int id, bool active, float size, Vec2 pos, Vec2 vel, float rot)
		: _id(id), _active(active), _size(size), _pos(pos), _vel(vel), _rot(rot) {}
};

int						asteroid_id{};
std::vector<ASTEROID>	asteroids_list;
static float			asteroid_timer = 0.f;


#include <chrono>

class DeltaTime {
public:
	DeltaTime() : lastFrameTime(std::chrono::steady_clock::now()) {}

	float GetDeltaTime() {
		auto currentTime = std::chrono::steady_clock::now();
		std::chrono::duration<float> deltaTime = currentTime - lastFrameTime;
		lastFrameTime = currentTime;
		return deltaTime.count();
	}
private:
	std::chrono::steady_clock::time_point lastFrameTime;
};

DeltaTime delta_time;

void spawnAsteroid(unsigned int count);

//=====================================================================================

std::map<std::string, SOCKET> client_port;

SOCKET udp_listener_socket;
std::mutex udp_mutex;

std::vector<ClientInfo> clients;

uint16_t numConnectedPlayers = 0;
const int MAX_PLAYERS = 4;

std::vector<Player> players(MAX_PLAYERS);
std::vector<std::vector<Bullet>> bullets(MAX_PLAYERS);

void SendToAllClients(const char* message)
{
	for (const auto& client : clients)
	{
		if (client.isConnected)
		{
			sendto(udp_listener_socket, message, strlen(message), 0, (SOCKADDR*)&client.address, sizeof(client.address));
		}
	}
}

void SendFloatToAllClients(float num)
{
	// Convert the float to a byte array
	char buffer[sizeof(float)];
	memcpy(buffer, &num, sizeof(float));

	for (const auto& client : clients)
	{
		if (client.isConnected)
		{
			sendto(udp_listener_socket, buffer, sizeof(buffer), 0, (SOCKADDR*)&client.address, sizeof(client.address));
		}
	}
}

void GetPosOfAllCLients()
{
	
}

void SendPlayersPosToAllClients()
{
	CMDID receive_id;
	std::vector<char> receive_buffer(4096);

	for (const auto& client : clients)
	{
		if (!client.isConnected) continue;

		int clientAddrSize = sizeof(client.address);
		int bytesReceived = recvfrom(udp_listener_socket, reinterpret_cast<char*>(receive_buffer.data()), receive_buffer.size(), 0, (SOCKADDR*)&client.address, &clientAddrSize);
		if (bytesReceived == SOCKET_ERROR) continue;

		memcpy(&receive_id, receive_buffer.data(), sizeof(receive_id));

		if (receive_id == SEND_PLAYERS)
		{
			Player receivedPlayer;
			Moves moves{};

			memcpy(&receivedPlayer, receive_buffer.data() + sizeof(receive_id), sizeof(Player));
			memcpy(&moves, receive_buffer.data() + sizeof(receive_id) + sizeof(Player), sizeof(Moves));

			int playerIndex = client.player_num - 1;

			players[playerIndex] = receivedPlayer;

			size_t bulletOffset = sizeof(receive_id) + sizeof(Player) + sizeof(Moves);
			size_t bulletSize = bytesReceived - bulletOffset;

			std::vector<Bullet> receivedBullets(bulletSize / sizeof(Bullet));
			memcpy(receivedBullets.data(), receive_buffer.data() + bulletOffset, bulletSize);
			bullets[playerIndex] = receivedBullets;

			if (moves.shoot)
			{
				memcpy(&receive_id, receive_buffer.data(), sizeof(receive_id));

				switch (receive_id)
				{
				case SEND_PLAYERS: {
					std::vector<Player> receivedPlayers(1);
					size_t expected_data_size = bytesReceived - sizeof(receive_id) - sizeof(Player);
					std::vector<Bullet> receivedBullets(expected_data_size/sizeof(Bullet));
					memcpy(receivedPlayers.data(), receive_buffer.data() + sizeof(receive_id), sizeof(Player));
					memcpy(receivedBullets.data(), receive_buffer.data() + sizeof(receive_id) + sizeof(Player), expected_data_size);

					players[client.player_num - 1].player_id = receivedPlayers[0].player_id;
					players[client.player_num - 1].position = receivedPlayers[0].position;
					players[client.player_num - 1].velocity = receivedPlayers[0].velocity;
					players[client.player_num - 1].direction = receivedPlayers[0].direction;
					players[client.player_num - 1].num_bullets = receivedPlayers[0].num_bullets;

					bullets[client.player_num - 1].resize(players[client.player_num - 1].num_bullets);
					for (int i{}; i < players[client.player_num - 1].num_bullets; ++i)
					{
						bullets[client.player_num - 1][i] = receivedBullets[i];
					}
					break;
				}

				case SEND_ASTEROIDS: {
					// Calculate how many asteroids are being received
					size_t asteroidCount = (bytesReceived - sizeof(receive_id)) / sizeof(ASTEROID);
					std::vector<ASTEROID> receivedAsteroids(asteroidCount);
					memcpy(receivedAsteroids.data(), receive_buffer.data() + sizeof(receive_id), asteroidCount * sizeof(ASTEROID));

					// Update or replace local asteroid list
					asteroids_list.clear(); // or update in place if desired
					for (const auto& asteroid : receivedAsteroids) {
						asteroids_list.push_back(asteroid);
					}

					break;
				}
				default:
					break;
				}
			}
		}
	}

	CMDID id = SEND_PLAYERS;
	size_t totalSize = sizeof(id) + MAX_PLAYERS * sizeof(Player);

	for (const auto& playerBullets : bullets)
	{
		totalSize += playerBullets.size() * sizeof(Bullet);
	}

	std::vector<char> combinedData(totalSize);
	char* bufferPtr = combinedData.data();

	memcpy(bufferPtr, &id, sizeof(id));
	bufferPtr += sizeof(id);

	memcpy(bufferPtr, players.data(), MAX_PLAYERS * sizeof(Player));
	bufferPtr += MAX_PLAYERS * sizeof(Player);

	for (const auto& playerBullets : bullets)
	{
		for (const auto& bullet : playerBullets)
		{
			memcpy(bufferPtr, &bullet, sizeof(Bullet));
			bufferPtr += sizeof(Bullet);
		}
	}

	for (const auto& client : clients)
	{
		if (client.isConnected)
		{
			sendto(udp_listener_socket, combinedData.data(), totalSize, 0, (SOCKADDR*)&client.address, sizeof(client.address));
		}
	}
}
void SendAsteroidDataToAllClients()
{
	if(asteroids_list.empty()) return;

	const int ID = 3;

	//Convert asteroid data to byte array
	int data_size = asteroids_list.size() * sizeof(ASTEROID);
	std::vector<char> buffer(data_size);

	// Copy the ID into the buffer
	memcpy(buffer.data(), &ID, sizeof(int));

	// Copy asteroid data into the buffer
	char* ptr = buffer.data() + sizeof(int);
	for (const auto& asteroid : asteroids_list) {
		memcpy(ptr, &asteroid, sizeof(ASTEROID));
		ptr += sizeof(ASTEROID);
	}

	// Send the byte array to all clients
	for (const auto& client : clients)
	{
		if (client.isConnected)
		{
			sendto(udp_listener_socket, buffer.data(), data_size, 0, (SOCKADDR*)&client.address, sizeof(client.address));
		}
	}

	asteroids_list.clear();
}

void SendBulletDataToAllClients() {

}

void HandleClientConnection(const sockaddr_in& clientAddr)
{
	// Find a client slot for the new client
	for (auto& client : clients)
	{
		if (!client.isConnected)
		{
			client.address = clientAddr;
			client.isConnected = true;
			break;
		}
	}

	// Check if all clients are connected
	bool allClientsConnected = true;
	for (const auto& client : clients)
	{
		if (!client.isConnected)
		{
			allClientsConnected = false;
			break;
		}
	}

	// If all clients are connected, send the game start signal
	if (allClientsConnected)
	{
		SendToAllClients("StartGame");
	}
}

int main()
{
	//std::cout << "Server UDP Port Number: ";
	uint16_t udp_port{9000};

	//std::cin >> udp_port;
	const std::string udp_port_string = std::to_string(udp_port);
	std::cout << std::endl;

	WSADATA wsaData{};

	int errorCode = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (errorCode != NO_ERROR)
	{
		std::cerr << "WSAStartup() failed." << std::endl;
		return errorCode;
	}

	addrinfo udp_hints{};
	SecureZeroMemory(&udp_hints, sizeof(udp_hints));
	udp_hints.ai_family = AF_INET;			// IPv4
	// For UDP use SOCK_DGRAM instead of SOCK_STREAM.
	udp_hints.ai_socktype = SOCK_DGRAM;	// Reliable delivery
	// Could be 0 for autodetect, but reliable delivery over IPv4 is always TCP.
	udp_hints.ai_protocol = IPPROTO_UDP;	// TCP
	// Create a passive socket that is suitable for bind() and listen().
	udp_hints.ai_flags = AI_PASSIVE;

	char hostName[NI_MAXHOST];
	if (gethostname(hostName, NI_MAXHOST) != 0) {
		std::cerr << "gethostname() failed." << std::endl;
		WSACleanup();
		return 2;
	}

	addrinfo* udp_info = nullptr;
	errorCode = getaddrinfo(hostName, udp_port_string.c_str(), &udp_hints, &udp_info);
	if ((errorCode) || (udp_info == nullptr))
	{
		std::cerr << "getaddrinfo() failed." << std::endl;
		WSACleanup();
		return errorCode;
	}

	char ipBuffer[NI_MAXHOST];
	char udp_port_buffer[NI_MAXSERV];
	errorCode = getnameinfo(
		udp_info->ai_addr,
		static_cast<int>(udp_info->ai_addrlen),
		ipBuffer,
		NI_MAXHOST,
		udp_port_buffer,
		NI_MAXSERV,
		NI_NUMERICHOST);

	if (errorCode != 0)
	{
		freeaddrinfo(udp_info);
		WSACleanup();
		return errorCode;
	}

	std::cout << "Server IP Address: " << ipBuffer << "\n";
	std::cout << "Server UDP Port Number: " << udp_port_buffer << "\n";

	HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
	DWORD mode;
	GetConsoleMode(hInput, &mode);
	SetConsoleMode(hInput, mode & (~ENABLE_ECHO_INPUT) & (~ENABLE_LINE_INPUT));

	udp_listener_socket = socket(
		udp_hints.ai_family,
		udp_hints.ai_socktype,
		udp_hints.ai_protocol);
	if (udp_listener_socket == INVALID_SOCKET)
	{
		std::cerr << "socket() failed." << std::endl;
		WSACleanup();
		return 1;
	}

	errorCode = bind(
		udp_listener_socket,
		udp_info->ai_addr,
		static_cast<int>(udp_info->ai_addrlen));
	if (errorCode != NO_ERROR)
	{
		std::cerr << "bind() failed." << std::endl;
		closesocket(udp_listener_socket);
		udp_listener_socket = INVALID_SOCKET;
	}

	char recvBuffer[1024];
	sockaddr_in clientAddr;
	int clientAddrSize = sizeof(clientAddr);

	bool game_start = false;

	while (true) 
	{
		if(!game_start)
		{
			int bytesReceived = recvfrom(udp_listener_socket, recvBuffer, sizeof(recvBuffer), 0, (SOCKADDR*)&clientAddr, &clientAddrSize);
			if (bytesReceived == SOCKET_ERROR)
			{
				std::cerr << "recvfrom failed with error: " << WSAGetLastError() << '\n';
				closesocket(udp_listener_socket);
				WSACleanup();
				return 1;
			}

			recvBuffer[bytesReceived] = '\0';
			std::cout << "Received message from client: " << recvBuffer << '\n';

			// Process the message and send a response
			std::string player_num = std::to_string(numConnectedPlayers);
			const char* responseMessage = player_num.c_str();
			sendto(udp_listener_socket, responseMessage, strlen(responseMessage), 0, (SOCKADDR*)&clientAddr, sizeof(clientAddr));
			numConnectedPlayers++;

			clients.push_back({ clientAddr, true, numConnectedPlayers});

			if(numConnectedPlayers >= MAX_PLAYERS)
			{
				std::string max_player_num = std::to_string(MAX_PLAYERS);
				const char* responseMessage = max_player_num.c_str();
				SendToAllClients(responseMessage);
				game_start = true;
			}
		}
		else
		{
			asteroid_timer += delta_time.GetDeltaTime();
			if (asteroid_timer > ASTEROID_TIME)
			{
				asteroid_timer -= ASTEROID_TIME;
				spawnAsteroid(1);
			}

			SendPlayersPosToAllClients();
			//SendAsteroidDataToAllClients();
		}

	}

	// Close the listener socket
	closesocket(udp_listener_socket);
	WSACleanup();
}

/*
* brief: get client's ip and port
* param: socket
* param: ip
* param: port
*/
void get_client_IP_and_port(SOCKET socket, char ip[NI_MAXHOST], char port[NI_MAXSERV])
{
	sockaddr_storage addr;
	int addrLen = sizeof(addr);

	// Retrieve the client's address information
	if (getpeername(socket, reinterpret_cast<sockaddr*>(&addr), &addrLen) == 0)
	{
		getnameinfo(reinterpret_cast<sockaddr*>(&addr),
			addrLen,
			ip,
			NI_MAXHOST,
			port,
			NI_MAXSERV,
			NI_NUMERICHOST | NI_NUMERICSERV);
	}
	else
	{
		std::cerr << "getpeername() failed." << std::endl;
	}
}

/*
*biref: respond to echo request
*param: socket
*param: message
*param: cmd_id
*/
int respond_echo(SOCKET socket, std::string const& message, CMDID cmd_id)
{
	//find destination port
	uint32_t ipAddrdest = *reinterpret_cast<const uint32_t*>(&message[sizeof(uint8_t)]);
	uint16_t portNumdest = ntohs(*reinterpret_cast<const uint16_t*>(&message[sizeof(uint8_t) + sizeof(uint32_t)]));

	// Convert IP address to string
	char ipBuffer[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &ipAddrdest, ipBuffer, sizeof(ipBuffer));
	std::string ip(ipBuffer);

	// Convert port number to string
	std::string port = std::to_string(portNumdest);

	std::string ip_port = ip;
	ip_port += ":";
	ip_port += port;

	if (!client_port.contains(ip_port))
	{
		std::vector<char> buffer(1);

		uint8_t cmdId = static_cast<uint8_t>(CMDID::ECHO_ERROR);
		std::memcpy(&buffer[0], &cmdId, sizeof(uint8_t));

		int err = send(socket, buffer.data(), static_cast<int>(buffer.size()), 0);
		return err;
	}

	SOCKET dest_socket = client_port.at(ip_port);

	// CMD ID: 1 byte
	// IP address: 4 bytes, in network byte order
	// Port number: 2 bytes, in network byte order
	// Text length: 4 bytes, in network byte order
	// Text: Variable length, as specified by the text length field

	size_t totalLength = message.length();
	std::vector<char> buffer(totalLength);

	uint8_t cmdId = static_cast<uint8_t>(cmd_id);
	std::memcpy(&buffer[0], &cmdId, sizeof(uint8_t));

	// Source IP address and port number
	sockaddr_in addr{};
	int addrLen = sizeof(addr);
	getpeername(socket, reinterpret_cast<sockaddr*>(&addr), &addrLen);

	uint32_t ipAddr = addr.sin_addr.s_addr;
	uint16_t portNum = ntohs(addr.sin_port);

	// Convert IP address to network byte order
	uint32_t ipAddrNet = ipAddr;
	// Copy IP address
	std::memcpy(&buffer[sizeof(uint8_t)], &ipAddrNet, sizeof(uint32_t));

	// Convert port number to network byte order
	uint16_t portNumNet = htons(portNum);
	// Copy port number
	std::memcpy(&buffer[sizeof(uint8_t) + sizeof(uint32_t)], &portNumNet, sizeof(uint16_t));

	// Calculate the length of the message part (excluding header fields)
	size_t messagePartLength = message.length() - sizeof(uint16_t) - sizeof(uint32_t) - sizeof(uint8_t);

	// Create a substring containing only the message part
	std::string messagePart = message.substr(sizeof(uint16_t) + sizeof(uint32_t) + sizeof(uint8_t), messagePartLength);

	//extract message from message part and convert to string
	std::string message_extracted = messagePart.substr(sizeof(uint32_t), messagePartLength - sizeof(uint32_t));

	// Copy the message part into the buffer
	std::memcpy(&buffer[sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint16_t)], messagePart.c_str(), messagePart.length());

	uint32_t total_length = static_cast<uint32_t>(message.length());
	// Send the message
	int err{};

	while (err < static_cast<int>(total_length))
	{
		//err = send(dest_socket, buffer.data(), static_cast<int>(buffer.size()), 0);
		int remainingBytes = static_cast<int>(total_length) - err;
		int bytesSent = send(dest_socket, buffer.data() + err, remainingBytes, 0);
		if (bytesSent <= 0) {
			std::cerr << "Error sending message." << std::endl;
			return -1;
		}
		err += bytesSent;
	}
	return err;
}

/*
*brief: respond to list user request
*param: socket
*/
int respond_list_user(SOCKET socket)
{
	// Prepare the message
	std::vector<char> buffer(3);

	uint16_t numUsers = static_cast<uint16_t>(client_port.size());
	uint16_t numUsersNetOrder = htons(numUsers);

	size_t offset = 3; // Start offset after commandId and numUsers

	size_t bufferSize;

	buffer[0] = static_cast<char>(RSP_LISTUSERS);

	// Number of users (assuming client_port is your map)
	std::memcpy(&buffer[1], &numUsersNetOrder, sizeof(uint16_t));

	// Resize the buffer to accommodate the maximum possible size
	bufferSize = buffer.size() + (client_port.size() * (sizeof(uint32_t) + sizeof(uint16_t)));
	buffer.resize(bufferSize);

	// Append each user's IP address and port number
	for (const auto& [ip_port, socket] : client_port)
	{
		size_t colon_pos = ip_port.find(':');
		if (colon_pos == std::string::npos)
		{
			std::cerr << "Invalid key format in client_port map: " << ip_port << std::endl;
			continue;
		}

		std::string ip = ip_port.substr(0, colon_pos);
		std::string port_str = ip_port.substr(colon_pos + 1);
		uint16_t port = static_cast<uint16_t>(std::stoi(port_str));

		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
		uint32_t ipAddr = addr.sin_addr.s_addr;
		uint16_t portNum = htons(port);

		// Append IP address
		std::memcpy(&buffer[offset], &ipAddr, sizeof(uint32_t));
		offset += sizeof(uint32_t);

		// Append port number
		std::memcpy(&buffer[offset], &portNum, sizeof(uint16_t));
		offset += sizeof(uint16_t);
	}

	int err = send(socket, buffer.data(), static_cast<int>(buffer.size()), 0);
	return err;
}

/*
*brief: send unknown command
*param: socket
*/
void send_unknown(SOCKET socket)
{
	std::vector<char> buffer(1);
	buffer[0] = static_cast<char>(UNKNOWN);
	send(socket, buffer.data(), static_cast<int>(buffer.size()), 0);
}

/*
*brief: send message
*param: socket
*param: commandId
*param: message
*/
int send_message(SOCKET socket, CMDID commandId, const std::string& message)
{
	switch (commandId)
	{
	case REQ_LISTUSERS:
		respond_list_user(socket);
		break;
	case UNKNOWN:
		send_unknown(socket);
		break;
	default:
		break;
	}

	return 0;
}

/*
*brief: remove client from map
*param: socket
*/
void remove_client(SOCKET socket)
{
	char ipBuffer[NI_MAXHOST];
	char portBuffer[NI_MAXSERV];

	get_client_IP_and_port(socket, ipBuffer, portBuffer);

	std::string ip_port = ipBuffer;
	ip_port += ":";
	ip_port += portBuffer;

	auto it = client_port.find(ip_port);
	if (it != client_port.end())
	{
		client_port.erase(it);
	}
}

/*
*brief: receive message
*param: socket
*param: message
*/
CMDID receive_message(SOCKET socket, const std::string& message)
{
	CMDID cmd_id = static_cast<CMDID>(message[0]);
	switch (cmd_id)
	{
	case REQ_QUIT:
		remove_client(socket);
		break;
	case REQ_LISTUSERS:
		send_message(socket, REQ_LISTUSERS, {});
		break;
	default:
		send_message(socket, UNKNOWN, {});
		break;
	}
	return cmd_id;
}

/*
*brief: handle message
*param: socket
*/
bool handle_message(SOCKET socket)
{
	bool stay = true;

	constexpr size_t BUFFER_SIZE = 50;
	char buffer[BUFFER_SIZE];

	std::string partial_message{};
	bool start_of_message{ true };
	uint32_t message_length{};

	while (true)
	{
		u_long mode = 1; // 1 for non-blocking mode, 0 for blocking mode
		ioctlsocket(socket, FIONBIO, &mode);
		const int bytesReceived = recv(
			socket,
			buffer,
			BUFFER_SIZE - 1,
			0);
		if (bytesReceived == SOCKET_ERROR)
		{
			size_t errorCode = WSAGetLastError();
			if (errorCode == WSAEWOULDBLOCK)
			{
				using namespace std::chrono_literals;
				std::this_thread::sleep_for(200ms);

				std::lock_guard<std::mutex> usersLock{ _stdoutMutex };
				continue;
			}
			break;
		}
		if (bytesReceived == 0)
		{
			break;
		}

		buffer[bytesReceived] = '\0';
		std::string text(buffer, bytesReceived);

		receive_message(socket, text);
	}

	shutdown(socket, SD_BOTH);

	closesocket(socket);

	return stay;
}

/*
*brief: disconnect
*param: listenerSocket
*/
void disconnect(SOCKET& listenerSocket)
{
	if (listenerSocket != INVALID_SOCKET)
	{
		shutdown(listenerSocket, SD_BOTH);
		closesocket(listenerSocket);
		listenerSocket = INVALID_SOCKET;
	}
}

float RandFloat()
{
	return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
}

void Vec2Set(Vec2* vec, float x, float y) {
	vec->x = x;
	vec->y = y;
}

void Vec2Scale(Vec2* result, const Vec2* vec, float scale) {
	result->x = vec->x * scale;
	result->y = vec->y * scale;
}

void spawnAsteroid(unsigned int count)
{
	for (unsigned int i = 0; i < count; ++i)
	{
		// for activating object instance 
		Vec2 pos, vel;

		// random size
		float size = RandFloat() * ASTEROID_SIZE + ASTEROID_SIZE;

		// random rotation
		float rot = RandFloat() * 360.0f;

		// random direction
		Vec2Set(&vel, cosf(rot), sinf(rot));

		// random speed 
		int dir = (int)(RandFloat() * 2.0f);
		switch (dir)
		{
		case 0:

		case 1:
			Vec2Scale(&vel, &vel, ASTEROID_SPEED + RandFloat() * ASTEROID_SPEED);
			break;

		case 2:
			Vec2Scale(&vel, &vel, ASTEROID_SPEED - RandFloat() * -ASTEROID_SPEED);
			break;

		default:
			break;

		}

		// random position,   outside the window
		// random window side
		int side = (int)(RandFloat() * 5.0f);

		// set half window width and height global variables
		float HALF_WIDTH = (float)WINDOW_WIDTH / 2.0f;	// half screen width 

		float HALF_HEIGHT = (float)WINDOW_HEIGHT / 2.0f;	// half screen height

		// spawn   selected side
		switch (side)
		{
		case 0: // fall through
		case 1: //   top left
			Vec2Set(&pos, (-RandFloat() * HALF_WIDTH - size), (HALF_HEIGHT + size));
			break;

		case 2: //   top right
			Vec2Set(&pos, (RandFloat() * HALF_WIDTH + size), (HALF_HEIGHT + size));
			break;

		case 3: //   bottom left
			Vec2Set(&pos, (-RandFloat() * HALF_WIDTH - size), (-HALF_HEIGHT - size));
			break;

		case 4:

		case 5: //   bottom right
			Vec2Set(&pos, (RandFloat() * HALF_WIDTH + size), (-HALF_HEIGHT - size));
			break;

		default:
			break;

		}

		// spawn asteroid by activating a game object instance
		ASTEROID asteroid(asteroid_id++,true, size, pos, vel, rot);
		asteroids_list.push_back(asteroid);
	}
}
