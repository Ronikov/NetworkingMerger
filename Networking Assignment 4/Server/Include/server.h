#pragma once
struct Bullet;

struct ClientInfo {
	sockaddr_in address;
	bool isConnected;
	uint16_t player_num;
};

struct Vec2
{
	float x = 0.f;
	float y = 0.f;
};

struct Player
{
	int player_id;
	bool shoot;
	Vec2 position;
	Vec2 velocity;
	float direction;
	int num_bullets;

	/*std::vector<Bullet> bullets;*/
};

struct Bullet
{
	int player_id;
	Vec2 position;
};

enum CMDID {
	UNKNOWN = static_cast<unsigned char>(0x0),
	REQ_QUIT = static_cast<unsigned char>(0x1),
	REQ_LISTUSERS = static_cast<unsigned char>(0x2),
	RSP_LISTUSERS = static_cast<unsigned char>(0x3),
	SEND_PLAYERS = static_cast<unsigned char>(0x4),
	RECEIVE_PLAYERS = static_cast<unsigned char>(0x5),
	SEND_ASTEROIDS = static_cast<unsigned char>(0x6),
	RECEIVE_ASTEROIDS = static_cast<unsigned char>(0x7),
	SEND_BULLETS = static_cast<unsigned char>(0x8),
	RECEIVE_BULLETS = static_cast<unsigned char>(0x9),

	CMD_TEST = static_cast<unsigned char>(0x20),
	ECHO_ERROR = static_cast<unsigned char>(0x30)
};

void get_client_IP_and_port(SOCKET socket, char ip[NI_MAXHOST], char port[NI_MAXSERV]);
int respond_echo(SOCKET socket, std::string const& message, CMDID cmd_id);
int respond_list_user(SOCKET socket);

void send_unknown(SOCKET socket);
int send_message(SOCKET socket, CMDID commandId, const std::string& message);
void remove_client(SOCKET socket);
CMDID receive_message(SOCKET socket, const std::string& message);
bool handle_message(SOCKET socket);
void disconnect(SOCKET& listenerSocket);