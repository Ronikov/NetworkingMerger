/******************************************************************************/
/*!
\file		GameState_Asteroids.h
\author     goh.a@digipen.edu

\date   	March 27 2025
\brief		GameState Manager function

Copyright (C) 2023 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
 /******************************************************************************/

#ifndef CSD1130_GAME_STATE_PLAY_H_
#define CSD1130_GAME_STATE_PLAY_H_

// ---------------------------------------------------------------------------
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

struct Player
{
	int player_id;
	AEVec2 position;
	AEVec2 velocity;
	float direction;
	int num_bullets;
};

struct Bullet
{
	int player_id;
	AEVec2 position;
	AEVec2 velocity;
	float direction;
};

struct Moves
{
	int player_id;
	bool shoot = false;
};

void GameStateAsteroidsLoad(void);
void GameStateAsteroidsInit(void);
void GameStateAsteroidsInitMultiPlayer(void);
void GameStateAsteroidsUpdate(void);
void GameStateAsteroidsDraw(void);
void GameStateAsteroidsFree(void);
void GameStateAsteroidsUnload(void);

void AsteroidsDataTransfer(SOCKET udp_socket);

// ---------------------------------------------------------------------------

#endif // CSD1130_GAME_STATE_PLAY_H_


