/**
* @file networking.cpp
* @author inigo fernandez , arenas.f , arenas.f@digipen.edu
* @date 2021/04/13
*
* This file contains the declarations of the classes and structs that are used in all
* the networking client and server
*/

#include "networking.hpp"
#include "game/network/client/client.hpp"
#include "game/network/server/server.hpp"
#include "game/TimeMgr/Time.h"
#include "game/state_ingame.h"

namespace network {

    /**
    * this function will start the network as a server or a client depending on the input
    * @param ip
    * @param port
    * @param mbserver
    * @param debug
    * @return  bool
    */
    void NetworkManager::Start(char const* ip, uint16_t port, bool mbserver, bool debug)
    {
        Im_server = mbserver;
        if (mbserver)    system = new server();
        else             system = new client();
        system->Start(ip, port, debug);
    }

    /**
    * this function will update the client or server depending on the current system
    * @return  bool
    */
    bool NetworkManager::Update()
    {
        return system->Update();
    }

    /**
    * this function will shutdown the network system
    * @return  void
    */
    void NetworkManager::ShutDown()
    {
        system->ShutDown();
        delete system;
    }

    /**
    * this function will process all the packets and alter the game itself depending on the packets
    * @param header
    * @param msg
    * @param data_length
    * @return  void
    */
    void NetworkManager::ProcessPacket(net_header header, char* msg, int data_length)
    {
        //cast the info
        net_flag flag = static_cast<net_flag>(header.flag);
        net_action action = static_cast<net_action>(header.type);

        //perform different operations depending on the packet
        switch (action)
        {
        case network::net_action::NET_PLAYER_NEW:
        {
            //create new player and new score
            net_player new_player;
            memcpy(&new_player, msg, sizeof(net_player));
            mGame.mShips[new_player.id] = mGame.gameObjInstCreate(TYPE_SHIP, SHIP_SIZE, &new_player.pos, 0, new_player.dir, true, new_player.id);
            mGame.mScores[new_player.id] = 0;
            break;
        }
        case network::net_action::NET_PLAYER_UPDATE:
        {
            //update the ship corresponding to the packet
            net_player new_player;
            memcpy(&new_player, msg, sizeof(net_player));
            if (mGame.mShips.find(new_player.id) == mGame.mShips.end())break;
            mGame.mShips[new_player.id]->posCurr = new_player.pos;
            mGame.mShips[new_player.id]->dirCurr = new_player.dir;
            break;
        }
        case network::net_action::NET_PLAYER_PRTCL_MOVE:
        {
            //create particles on the ship that is moving forward
            float dir = 0;
            vec2 pos;
            memcpy(&dir, msg, sizeof(float));
            memcpy(&pos, msg + sizeof(float), sizeof(vec2));
            mGame.sparkCreate(PTCL_EXHAUST, &pos, 2, dir + 0.8f * PI, dir + 1.2f * PI);
            break;
        }
        case network::net_action::NET_PLAYER_DEATH:
        {
            //destroy the ship of the player that dead
            GameObjInst* ship = mGame.mShips[header.id];
            if (ship)
                mGame.gameObjInstDestroy(ship);
            mGame.mShips.erase(header.id);
            break;
        }
        case network::net_action::NET_SCORE_UPDATE:
        {
            //get the scores count
            int scores_count = 0;
            memcpy(&scores_count, msg, 4);

            //update all the scores
            for (int i = 0; i < scores_count; i++)
            {
                int id = 0;
                int score = 0;
                std::memcpy(&id, msg + 2 * i * sizeof(int) + 4, sizeof(int));
                std::memcpy(&score, msg + sizeof(int) + 2 * i * sizeof(int) + 4, sizeof(int));
                mGame.mScores[id] = score;
            }
            break;
        }
        case network::net_action::NET_PLAYER_SHOT:
        {
            //create a bullet
            auto* ship = mGame.mShips[header.id];
            vec2 vel = { glm::cos(ship->dirCurr), glm::sin(ship->dirCurr) };
            vel = vel * BULLET_SPEED;
            mGame.gameObjInstCreate(TYPE_BULLET, BULLET_SIZE, &ship->posCurr, &vel, ship->dirCurr, true, header.id);
            break;
        }
        case network::net_action::NET_PLAYER_BOMB:
        {
            //create a bomb
            auto* ship = mGame.mShips[header.id];
            mGame.gameObjInstCreate(TYPE_BOMB, BOMB_SIZE, &ship->posCurr, 0, 0, true, header.id);
            break;
        }
        case network::net_action::NET_PLAYER_MISSILE:
        {
            //create a missile
            auto* ship = mGame.mShips[header.id];
            float dir = ship->dirCurr;
            vec2  vel = ship->velCurr;
            vec2  pos;

            pos = { glm::cos(ship->dirCurr), glm::sin(ship->dirCurr) };
            pos = pos * ship->scale * 0.5f;
            pos = pos + ship->posCurr;

            mGame.gameObjInstCreate(TYPE_MISSILE, 1.0f, &pos, &vel, dir, true, header.id);
            break;
        }
        case network::net_action::NET_ASTEROID_UPDATE:
        {
            //update all the asteroids
            int asteroid_count = 0;
            memcpy(&asteroid_count, msg, sizeof(int));

            for (int i = 0; i < asteroid_count; i++)
            {
                //get the current asteroid information
                net_asteroid asteroid;
                memcpy(&asteroid, msg + 4 + i * sizeof(net_asteroid), sizeof(net_asteroid));

                //find the asteroid
                auto it = mGame.mAsteroids.find(asteroid.id);
                GameObjInst* ast = nullptr;

                //if the asteroid is not found then is a new one that we need to create
                if (it == mGame.mAsteroids.end())
                {
                    ast = mGame.astCreate(0);
                    mGame.mAsteroids[asteroid.id] = ast;
                }
                else
                    ast = it->second;

                //update the asteroid values
                ast->m_id = asteroid.id;
                ast->life = asteroid.life;
                ast->posCurr = asteroid.pos;
                ast->scale = asteroid.scale;
            }
            break;
        }
        case network::net_action::NET_ASTEROID_DESTROY:
        {
            //destroy an asteroid
            net_explosion exp;
            memcpy(&exp, msg, sizeof(net_explosion));

            //get the type of explosion
            uint32_t type = static_cast<uint32_t>(exp.exp_type);

            //if a ship exploded in that position
            if (type == PTCL_EXPLOSION_L)
                mGame.sparkCreate(PTCL_EXPLOSION_L, &exp.pos, 100, 0.0f, 2.0f * PI);

            //else destroy the asteroid with the needed information
            else if (type == PTCL_EXPLOSION_M)
            {
                if(mGame.mAsteroids.find(exp.ast_id) != mGame.mAsteroids.end())
                {
                    GameObjInst* ast = mGame.mAsteroids[exp.ast_id];
                    mGame.gameObjInstDestroy(ast);

                    if(exp.bullet_type == 0)
                        mGame.sparkCreate(PTCL_EXPLOSION_M, &exp.pos, (uint32_t)(exp.scale * 10), exp.dir - 0.05f * PI, exp.dir + 0.05f * PI, exp.scale);
                    else
                        mGame.sparkCreate(PTCL_EXPLOSION_M, &exp.pos, 20, exp.dir + 0.4f * PI, exp.dir + 0.45f * PI);
                }
            }
            break;
        }
        case network::net_action::NET_PLAYER_DISCONECTS:
        {
            //if a player disconects remove his stats
            RemovePlayer(header.id);
            break;
        }
        case network::net_action::NET_GAME_OVER:
        {
            //set the game state
            mGame.game_ended = true;
            break;
        }
        case network::net_action::NET_GAME_WON:
        {
            //update the stats
            int won_id = 0;
            memcpy(&won_id, msg, sizeof(int));
            mGame.won_id = won_id;
            mGame.game_won = true;

            //remove all the players
            for (auto& it : mGame.mShips)
                mGame.gameObjInstDestroy(it.second);
            mGame.mShips.clear();
            mGame.spShip = nullptr;
            break;
        }
        default:
            break;
        }
    }

    /**
    * this function will create a packet depedning on the input
    * @param action
    * @param expected_answer
    * @param data
    * @param data_length
    * @return  void
    */
    void NetworkManager::BroadCastMsg(net_action action, bool expected_answer, char* data, int data_length)
    {
        switch (action)
        {
        case network::net_action::NET_PLAYER_UPDATE:
        {
            //get the curren tplayer info and send it
            net_player player = GetPlayerInfo();
            std::vector<char> msg(sizeof(net_player));
            memcpy(msg.data(), &player, sizeof(net_player));
            system->SendMsg(net_flag::NET_SEQ, net_action::NET_PLAYER_UPDATE, system->m_id, ++system->m_seq, expected_answer, msg.data(), sizeof(net_player));
            break;
        }
        case network::net_action::NET_PLAYER_PRTCL_MOVE:
        {
            system->SendMsg(net_flag::NET_SEQ, net_action::NET_PLAYER_PRTCL_MOVE, system->m_id, ++system->m_seq, expected_answer, data, data_length);
            break;
        }
        case network::net_action::NET_PLAYER_DEATH:
        {
            system->SendMsg(net_flag::NET_SEQ, net_action::NET_PLAYER_DEATH, system->m_id, ++system->m_seq, expected_answer);
            break;
        }
        case network::net_action::NET_SCORE_UPDATE:
        {
            //get all the scores info and send it
            auto players_count = mGame.mScores.size();
            std::vector<char> msg(2*players_count*sizeof(int) + 4);

            std::memcpy(msg.data(), &players_count, sizeof(int));
            int i = 0;
            for (auto& it : mGame.mScores)
            {
                std::memcpy(msg.data() + 2 * i * sizeof(int) + 4, &(it.first), sizeof(int));
                std::memcpy(msg.data() + sizeof(int) + 2 * i * sizeof(int) + 4, &(it.second), sizeof(int));
                i++;
            }
            system->SendMsg(net_flag::NET_SEQ, net_action::NET_SCORE_UPDATE, system->m_id, ++system->m_seq, expected_answer, msg.data(), (int)msg.size());
            break;
        }
        case network::net_action::NET_PLAYER_SHOT:
        {
            system->SendMsg(net_flag::NET_SEQ, net_action::NET_PLAYER_SHOT, system->m_id, ++system->m_seq, expected_answer);
            break;
        }
        case network::net_action::NET_PLAYER_BOMB:
        {
            system->SendMsg(net_flag::NET_SEQ, net_action::NET_PLAYER_BOMB, system->m_id, ++system->m_seq, expected_answer);
            break;
        }
        case network::net_action::NET_PLAYER_MISSILE:
        {
            system->SendMsg(net_flag::NET_SEQ, net_action::NET_PLAYER_MISSILE, system->m_id, ++system->m_seq, expected_answer);
            break;
        }
        case network::net_action::NET_ASTEROID_NEW:
        {
            system->SendMsg(net_flag::NET_SEQ, net_action::NET_ASTEROID_NEW, system->m_id, ++system->m_seq, expected_answer, data, data_length);
            break;
        }
        case network::net_action::NET_ASTEROID_UPDATE:
        {
            std::vector<char> ast_msg = AllAsteroidsPacketCreate();
            system->SendMsg(net_flag::NET_SEQ, net_action::NET_ASTEROID_UPDATE, system->m_id, ++system->m_seq, expected_answer, ast_msg.data(), (int)ast_msg.size());
            break;
        }
        case network::net_action::NET_ASTEROID_DESTROY:
        {
            system->SendMsg(net_flag::NET_SEQ, net_action::NET_ASTEROID_DESTROY, system->m_id, ++system->m_seq, expected_answer, data, data_length);
            break;
        }
        case network::net_action::NET_PLAYER_DISCONECTS:
        {
            system->SendMsg(net_flag::NET_SEQ, net_action::NET_PLAYER_DISCONECTS, system->m_id, ++system->m_seq, expected_answer);
            break;
        }
        case network::net_action::NET_GAME_OVER:
        {
            system->SendMsg(net_flag::NET_SEQ, net_action::NET_GAME_OVER, system->m_id, ++system->m_seq, expected_answer);
            break;
        }
        case network::net_action::NET_GAME_WON:
        {
            system->SendMsg(net_flag::NET_SEQ, net_action::NET_GAME_WON, system->m_id, ++system->m_seq, expected_answer);
            break;
        }
        default:
            break;
        }
    }

    /**
    * this function will return the info of all the ships
    * @param new_ship
    * @param new_pos
    * @return  void
    */
    std::vector<char> NetworkManager::AllShipsPacketCreate(bool new_ship, vec2 new_pos)
    {
        //get the number of ships
        auto ships_count = mGame.mShips.size();
        auto message_size = ships_count * (sizeof(net_player)) + 4;

        //if new ship we will add the position of the new ship
        if (new_ship) message_size += sizeof(vec2);

        //datagram that will store all the information
        std::vector<char> msg(message_size);

        //copy the number of ships
        memcpy(msg.data(), &ships_count, 4);

        //copy all the ships positions and id
        unsigned i = 0;
        for (auto& it : mGame.mShips)
        {
            net_player player(it.first, it.second->dirCurr, it.second->posCurr);
            memcpy(msg.data() + 4 + i * sizeof(net_player), &player, sizeof(net_player));
            i++;
        }

        //set the new position if its a new ship
        if(new_ship)
            memcpy(msg.data() + message_size - sizeof(vec2), &new_pos, sizeof(vec2));

        //return the msg
        return msg;
    }

    /**
    * this function will process the info of all the ships
    * @param header
    * @param data
    * @param new_ship
    * @return  void
    */
    void NetworkManager::AllShipsPacketProcess(net_header header, char* data, bool new_ship)
    {
        //get the number of players
        int ships_count = 0;
        memcpy(&ships_count, data, 4);

        for (int i = 0; i < ships_count; i++)
        {
            //get the player info
            net_player player;
            memcpy(&player, data + 4 + i * sizeof(net_player), sizeof(net_player));
            
            //if the player doesnt exist create it
            auto it = mGame.mShips.find(player.id);
            if (it == mGame.mShips.end())
                CreateShip(player);

            //else update players info
            else
            {
                mGame.mShips[player.id]->posCurr = player.pos;
                mGame.mShips[player.id]->dirCurr = player.dir;
            }
        }
        //if there is no ship in the game we will create a new one for the new client
        if (new_ship)
        {
            vec2 new_pos = {};
            memcpy(&new_pos, data + 4 + ships_count * sizeof(net_player), sizeof(vec2));
            mGame.spShip = mGame.gameObjInstCreate(TYPE_SHIP, SHIP_SIZE, &new_pos, 0, 0.0f, true, system->m_id);
            mGame.mShips[header.id] = mGame.spShip;
        }
    }

    /**
    * this function will create a new ship and return that info
    * @param player
    * @return  std::vector<char>
    */
    std::vector<char> NetworkManager::CreateShip(net_player player)
    {
        std::vector<char> new_ship(sizeof(net_player));
        mGame.mShips[player.id] = mGame.gameObjInstCreate(TYPE_SHIP, SHIP_SIZE, &player.pos, 0, player.dir, true, player.id);
        mGame.mScores[player.id] = 0;

        memcpy(new_ship.data(), &player, sizeof(net_player));
        return new_ship;
    }

    /**
    * this function will return the info of all the asteroids
    * @return  std::vector<char>
    */
    std::vector<char> NetworkManager::AllAsteroidsPacketCreate()
    {
        auto ast_count = mGame.mAsteroids.size();
        std::vector<char> msg(ast_count * sizeof(net_asteroid) + 4);

        //set the number of asteroids
        memcpy(msg.data(), &ast_count, sizeof(int));

        //set all the info of evey asteroid
        int i = 0;
        for (auto& it : mGame.mAsteroids)
        {
            net_asteroid ast(it.second->m_id, it.second->life, it.second->scale, it.second->posCurr);
            memcpy(msg.data() + sizeof(int) + i * sizeof(net_asteroid), &ast, sizeof(net_asteroid));
            i++;
        }

        return msg;
    }

    /**
    * this function will return the info of the player that is playing
    * @return  void
    */
    net_player NetworkManager::GetPlayerInfo()
    {
        net_player mPlayer;
        mPlayer.id = system->m_id;
        mPlayer.dir = mGame.spShip->dirCurr;
        mPlayer.pos = mGame.spShip->posCurr;
        return mPlayer;
    }

    /**
    * this function will remove the players stats of the id player
    * @param player_id
    * @return  void
    */
    void NetworkManager::RemovePlayer(int player_id)
    {
        //check if exist
        auto it = mGame.mShips.find(player_id);
        if (it == mGame.mShips.end()) return;

        //remove the player stats from the game
        mGame.gameObjInstDestroy(mGame.mShips[player_id]);
        mGame.mShips.erase(player_id);
        mGame.mScores.erase(player_id);
    }

    /**
    * this function will create a new packet with the details of the header provided and the message provided as data
    * @param flag                   - flag of the operation
    * @param action                 - action that correspond to this packet
    * @param seq_num                - sequence number corresponding to this packet
    * @param expected_acknowledge   - bool to check if it needs to get acknowledge
    * @param msg                    - data that will have the packet
    * @param size                   - size of the data
    * @return  void
    */
    void BaseNetwork::SendMsg(net_flag flag, net_action action, int id, int seq_num, bool expected_acknowledge, const char* msg, int size)
    {
        //datagram that will store all the information
        std::vector<char> send_buffer(sizeof(net_header) + size);

        //initialize the header
        net_header header = CreateHeader(flag, action, expected_acknowledge, id, seq_num);

        //set the header and the message in a single datagram
        memcpy(send_buffer.data(), &header, sizeof(header));
        memcpy(send_buffer.data() + sizeof(header), msg, size);

        //store it in the map if an acknowledge from the server is expected
        if (expected_acknowledge)
        {
            //add the new packet into the map of packets
            std::pair<float, std::vector<char>> info{ 0.0f, send_buffer };
            sended_packets[seq_num] = info;
        }

        //send the message
        sendto(m_socket, send_buffer.data(), static_cast<int>(send_buffer.size()), 0, reinterpret_cast<sockaddr*>(&m_remote_endpoint), sizeof(m_remote_endpoint));
    }

    /**
    * this function will create a header for the filetransfer packet with the data provided
    * @param flag                   - flag of the operation
    * @param action                 - action that correspond to this packet
    * @param sequence               - sequence number corresponding to this packet
    * @return  net_header
    */
    net_header BaseNetwork::CreateHeader(net_flag flag, net_action action, bool expected_ack, int id, int sequence)
	{
		//initialize the header
        net_header header = {};
        memcpy(&header.flag, &flag, 1);
        memcpy(&header.type, &action, 1);
        memcpy(&header.expect_ack, &expected_ack, 1);
        memcpy(&header.sequence, &sequence, 4);
        memcpy(&header.id, &id, 4);

		return header;
	}

    /**
    * this function will unpack the header of the packet and the data of the packet and store them in the 
    * variables provided
    * @param packet                 - full packet information
    * @param data_length            - size of the packet
    * @param recv_header            - variable to store the information of the header
    * @param msg                    - array to store the data from the packet
    * @return  void
    */
	void BaseNetwork::UnpackPacket(std::vector<char> packet, int data_length, net_header& recv_header, char* msg)
	{
		//get the info of the header and the message separated from the packet
		memcpy(&recv_header, packet.data(), sizeof(recv_header));
		memcpy(msg, packet.data() + sizeof(recv_header), data_length - sizeof(recv_header));
	}

    /**
    * this function will update the packets that are waiting for acknowledge
    * @return  void
    */
    void BaseNetwork::UpdateSendedPackets()
    {
        //update the current_alive_timer since we didnt get a packet yet
        current_alive_time += TimeMgr.GetDt();

        //update the timer of each of the packets
        for (auto& it : sended_packets)
        {
            //update the timer and check if we reach to the time to resend the packet
            it.second.first += TimeMgr.GetDt();
            if (it.second.first >= acknowledge_timer.count())
            {
                //resend the packet
                sendto(m_socket, it.second.second.data(), static_cast<int>(it.second.second.size()), 0, reinterpret_cast<sockaddr*>(&m_remote_endpoint), sizeof(m_remote_endpoint));
                it.second.first = 0.0f;
            }
        }
    }
}

