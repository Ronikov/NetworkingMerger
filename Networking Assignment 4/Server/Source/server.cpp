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

//std::vector<Vec2> players_pos(MAX_PLAYERS);
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

void SendDataToAllClients()
{
	CMDID receive_id;
	std::vector<char> receive_buffer(4096);

	for (const auto& client : clients)
	{
		if (!client.isConnected)
			continue;

		int clientAddrSize = sizeof(client.address);
		int bytesReceived = recvfrom(
			udp_listener_socket,
			reinterpret_cast<char*>(receive_buffer.data()),
			receive_buffer.size(),
			0,
			(SOCKADDR*)&client.address,
			&clientAddrSize
		);

		if (bytesReceived == SOCKET_ERROR)
			continue;

		memcpy(&receive_id, receive_buffer.data(), sizeof(receive_id));

		switch (receive_id)
		{
		case SEND_PLAYERS:
		{
			// Extract Player and Bullet data from the buffer
			std::vector<Player> receivedPlayers(1);
			memcpy(receivedPlayers.data(), receive_buffer.data() + sizeof(receive_id), sizeof(Player));

			// Update server state for that client
			int index = client.player_num - 1;
			players[index] = receivedPlayers[0];
			break;
		}
		case SEND_BULLETS:
		{
			break;
		}
		default:
			break;
		}
	}

	// Prepare combined data packet to send back to all clients
	CMDID send_id = SEND_PLAYERS;
	size_t totalSize = sizeof(send_id) + MAX_PLAYERS * sizeof(Player);

	std::vector<char> combinedData(totalSize);
	char* bufferPtr = combinedData.data();

	// Pack CMDID
	memcpy(bufferPtr, &send_id, sizeof(send_id));
	bufferPtr += sizeof(send_id);

	// Pack Player data
	memcpy(bufferPtr, players.data(), MAX_PLAYERS * sizeof(Player));
	bufferPtr += MAX_PLAYERS * sizeof(Player);

	// Broadcast combined data to all connected clients
	for (const auto& client : clients)
	{
		if (client.isConnected)
		{
			sendto(
				udp_listener_socket,
				combinedData.data(),
				totalSize,
				0,
				(SOCKADDR*)&client.address,
				sizeof(client.address)
			);
		}
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

			SendDataToAllClients();
		}

	}

	// Close the listener socket
	closesocket(udp_listener_socket);
	WSACleanup();
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
