#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdint>

struct Vec2 {
    float x = 0.f;
    float y = 0.f;
};

struct Bullet {
    int player_id;
    Vec2 position;
};

struct Player {
    int player_id;
    bool shoot;
    Vec2 position;
    Vec2 velocity;
    float direction;
    int num_bullets;
};

struct ClientInfo {
    sockaddr_in address;
    bool isConnected = false;
    uint16_t player_num = 0;
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
