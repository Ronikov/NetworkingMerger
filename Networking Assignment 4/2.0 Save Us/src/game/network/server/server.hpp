#pragma once
/**
* @file server.hpp
* @author inigo fernandez , arenas.f , arenas.f@digipen.edu
* @date 2020/03/19
*
* This file contains the declartion of the server that will be listening for a new 
* client 
*/

#include "game/network/system/networking.hpp"
#include <cinttypes>

namespace network {

    
    class servers_client;
    class server : public BaseNetwork
    {
    public:
        void SendMsg(net_flag flag, net_action action, int id, int seq_num = 0, bool expected_acknowledge = true, const char* msg = nullptr, int size = 0);

      private:
        //vector of clients
        int clients_count = 0;

        servers_client* CheckDuplicateClient(sockaddr_in const& _remote_address);
        void ConnectClient(net_header recv_header, char* msg, sockaddr_in const& _remote_address);
        void ProcessPacket(net_header recv_header, char* msg, int data_length);
        void RemoveClient(int client_id);
        
      public:
        void Start(char const* ip, uint16_t port, bool debug = false);
        void ShutDown();
        bool Update();
        std::unordered_map<int, servers_client*> mClients;
    };

    class servers_client : public BaseNetwork
    {
        friend server;
    public:
        servers_client(sockaddr_in const& _remote_address, SOCKET s, server* server);
        bool Update();

    private:
        server* mServer = nullptr;
        bool requested_connection = false;
        bool connected = false;
    };
}