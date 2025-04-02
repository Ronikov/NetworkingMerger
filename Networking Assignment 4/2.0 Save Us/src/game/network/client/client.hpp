#pragma once
/**
* @file client.hpp
* @author inigo fernandez , arenas.f , arenas.f@digipen.edu
* @date 2020/03/19
*
* This file contains the declaration of the client class that will connect to the server
*/

#include "game/network/system/networking.hpp"
#include <cinttypes>
#include <vector>

namespace network 
{
    class client : public BaseNetwork
    {
      private:
        bool ProcessPacket(std::vector<char> packet, int data_length);
        bool ConnectToServer();

      public:
        void Start(char const* ip, uint16_t port, bool debug = false);
        bool Update();
        void ShutDown();
    };
}