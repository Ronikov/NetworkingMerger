/**
* @file networking.hpp
* @author inigo fernandez , arenas.f , arenas.f@digipen.edu
* @date 2021/04/13
*
* This file contains the declarations of the classes and structs that are used in all 
* the networking client and server
*/

#pragma once
#include "utilsnetwork.hpp"
#include <vector>
#include <unordered_map>
#include <queue>
#include "engine/math.hpp"

namespace network {

    struct net_explosion
    {
        int ast_id = 0;
        int exp_type = 0;
        int bullet_type = 0;
        float scale = 0;
        float dir = 0;
        vec2 pos = {};
    };

    struct net_player
    {
        int id = 0;
        float dir = 0;
        vec2 pos = {};
    };

    struct net_asteroid
    {
        int id = 0;
        float life = 0;
        float scale = 0;
        vec2 pos = {};
    };

    //header of the packets
    struct net_header
    {
        char        flag;
        char        type;
        char        expect_ack;
        char        padding[1];
        int         sequence;
        int         id;
    };
    static_assert(sizeof(net_header) == 12);

    //flg for the packet
    enum class net_flag
    {
        NET_FIN       = 0,
        NET_SYN       = 1,
        NET_ACK       = 2,
        NET_SYN_ACK   = 3,
        NET_SEQ       = 4
    };

    //action of the packet
    enum class net_action
    {
        NET_CONECTION,
        NET_PLAYER_NEW,
        NET_PLAYER_UPDATE,
        NET_PLAYER_DEATH,
        NET_SCORE_UPDATE,
        NET_PLAYER_SHOT,
        NET_PLAYER_BOMB,
        NET_PLAYER_MISSILE,
        NET_ASTEROID_UPDATE,
        NET_ASTEROID_NEW,
        NET_ASTEROID_DESTROY,
        NET_PLAYER_PRTCL_MOVE,
        NET_PLAYER_DISCONECTS,
        NET_GAME_OVER,
        NET_GAME_WON
    };

    //maximum amount of data a packet can send
    const unsigned  MAX_PAYLOAD_SIZE = 1024 - sizeof(net_header);

    // used to allow reordering
    using pair_timers_data = std::pair<float, std::vector<char>>;

    //system tht has common operations between server and client
    class BaseNetwork
    {
    public:
        //id of the packet that is evaluated
        bool                 mbdebug = false;
        int                  m_id = 0;
        int                  m_seq = 0;

    protected:
        //variables of the client
        SOCKET               m_socket{ INVALID_SOCKET };
        sockaddr_in          m_remote_endpoint = {};

        //storage object for the packets sended that need acknowledge 
        std::unordered_map<int, pair_timers_data> sended_packets;

        //timers to check with
        std::chrono::nanoseconds alive_timer{ std::chrono::seconds(20) };
        std::chrono::nanoseconds acknowledge_timer{ std::chrono::seconds(2) };
        float current_alive_time = 0.0f;

    public:
        virtual void Start(char const* ip, uint16_t port, bool debug = false) {}
        virtual bool Update() { return false; }
        virtual void ShutDown() {}

        //------------------------------------FUNCTIONS----------------------------------------------
        net_header CreateHeader(net_flag flag, net_action action, bool expected_ack, int id, int sequence = 0);
        void UpdateSendedPackets();
        void UnpackPacket(std::vector<char> packet, int data_length, net_header& recv_header, char* msg);
        virtual void SendMsg(net_flag flag, net_action action,int id, int seq_num = 0, bool expected_acknowledge = true, const char* msg = nullptr, int size = 0);
    };


    class NetworkManager
    {
    public:
        //destructor that destorys everything
        ~NetworkManager() {}

        void Start(char const* ip, uint16_t port, bool mbserver, bool debug);
        bool Update();
        void ShutDown();
        BaseNetwork* system = nullptr;
        bool Im_server = false;

        //make the network a singleton
        static NetworkManager& Instance()
        {
            static NetworkManager profiler;
            return profiler;
        }

        void ProcessPacket(net_header header, char* msg, int data_length);
        void BroadCastMsg(net_action action, bool expected_answer = false, char* data = nullptr, int data_length = 0);
        void RemovePlayer(int player_id);

        //usefull functions in the game
        std::vector<char> AllAsteroidsPacketCreate();
        std::vector<char> AllShipsPacketCreate(bool new_ship = false, vec2 new_pos = {});
        void AllShipsPacketProcess(net_header header, char* data, bool new_ship = false);
        std::vector<char> CreateShip(net_player player);
    private:
        //constructor of the network manager
        NetworkManager() {}
        net_player GetPlayerInfo();
    };
}

#define NetMgr (network::NetworkManager::Instance())