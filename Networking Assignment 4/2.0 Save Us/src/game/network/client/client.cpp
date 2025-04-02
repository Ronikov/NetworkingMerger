/**
* @file client.cpp
* @author inigo fernandez , arenas.f , arenas.f@digipen.edu
* @date 2020/03/19
*
* This file contains the implementation of the client in UDP
*/

#include "client.hpp"
#include <cassert>
#include <cstring>
#include <sstream>
#include <chrono>
#include "game/game.hpp"
#include "game/TimeMgr/Time.h"
#include "game/network/system/networking.hpp"

namespace network {

    /**
    * Update of the client that will wait for incoming data to store in a file if downloading
    * or send data from a file to the server if downloading and wait for acknowledge of each 
    * of the packets
    * @return  bool
    */
    bool client::Update()
    {
        //bool to check if the simulation should continue
        bool alive = true;

        //check if the server didnt sent any messages for a long period of time so we assume that there was a problem therefor we disconnect
        if (current_alive_time > alive_timer.count())
        {
            if(mbdebug) std::cout << "Server Response Time Out" << std::endl;
            SendMsg(net_flag::NET_FIN, net_action::NET_CONECTION, m_id, m_seq, false);
            return false;
        }

        //integer for the recv
        int err = 1;
        //iterate until we read and process all the datagrams in the net
        while (err > 0)
        {
            //recv a new packet
            std::vector<char> recv_buffer;
            recv_buffer.resize(MAX_PAYLOAD_SIZE + sizeof(net_header));
            int remote_size = sizeof(m_remote_endpoint);
            int err = recvfrom(m_socket, recv_buffer.data(), static_cast<int>(recv_buffer.size()), 0, reinterpret_cast<sockaddr*>(&m_remote_endpoint), &remote_size);

            //check for errors in recv
            if (err == -1)
            {
                err = WSAGetLastError();
                //if there is no error update the timers of the sended packets
                if (err == WSAEWOULDBLOCK)
                    UpdateSendedPackets();

                //else we stop the connection
                else {
                    if(mbdebug) std::cout << "Error in recv" << std::endl;
                    alive = false;
                }
                break;
            }
            //we recieved a packet so we need to process it
            else
                alive = ProcessPacket(recv_buffer, err);
        }
        return alive;
    }

    /**
    * this function will start a connection with in the address and port provided and set the client to only listen if the last bool is set
    * @param ip         - address to connect to
    * @param port       - port to connect to
    * @param quiet      - bool to set if the client will only listen to messages
    * @param download   - check if the client will upload or download
    * @param debug      - check to print more debug information
    * @return  void
    */
    void client::Start(char const* ip, uint16_t port, bool debug)
    {
        //set quiet flag
        mbdebug = debug;

        //initialize the winsock library
        network_create();

        //create socket and check that the creation didnt fail
        m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        assert(m_socket != INVALID_SOCKET);

        //make the socket non blocking
        u_long blocking_mode = 1;
        int err = ioctlsocket(m_socket, FIONBIO, &blocking_mode);
        assert(err == NO_ERROR);

        //convert IP from string to network address
        m_remote_endpoint.sin_family = AF_INET;
        m_remote_endpoint.sin_addr = cstr_to_ipv4(ip);
        m_remote_endpoint.sin_port = htons(port);

        //connect to the server
        ConnectToServer();
    }

    /**
    * this function will close the socket of the client and terminate the winsock program
    * @return  void
    */
    void client::ShutDown()
    {
        //send the message to end the connection to the server
        SendMsg(net_flag::NET_FIN, net_action::NET_CONECTION, m_id, 0);

        //release
        closesocket(m_socket);
        network_destroy();

        //clean the map and queue 
        sended_packets.clear();
    }

    /**
    * this function will try to connect to the server and send it the type of action and file that will get processed
    * @return  bool
    */
    bool client::ConnectToServer()
    {
        //send the requesto of connection to the server
        SendMsg(net_flag::NET_SYN, net_action::NET_CONECTION, m_id, 0);
        std::cout << "Sending SYN: " << std::endl;

        //iterate until connection is made or dismised
        bool exit = false;
        while (!exit)
        {
            //check if there was no update in a lot of time so we assume that the server disconected
            if (current_alive_time > alive_timer.count())
                return false;

            //check for server to acknowledge the connection
            std::vector<char> recv_buffer;
            recv_buffer.resize(MAX_PAYLOAD_SIZE + sizeof(net_header));
            int err = recvfrom(m_socket, recv_buffer.data(), static_cast<int>(recv_buffer.size()), 0, nullptr, nullptr);

            //check if error happened
            if (err == -1)
            {
                err = WSAGetLastError();
                if (err == WSAEWOULDBLOCK)
                {
                    //update timer until we ask server for connection again
                    current_alive_time += TimeMgr.GetDt();
                    if (current_alive_time >= acknowledge_timer.count())
                    {
                        //if timer passed we ask again the server for new connection
                        SendMsg(net_flag::NET_SYN, net_action::NET_CONECTION, m_id, 0);
                        current_alive_time = 0.0f;
                    }
                }
                //else we stop the connection
                else {
                    std::cout << "Error in recv" << std::endl;
                    return false;
                }
            }
            else
            {
                //process the packet recieved
                net_header recv_header = {};
                char message[MAX_PAYLOAD_SIZE] = { 0 };
                UnpackPacket(recv_buffer, err, recv_header, message);
                sended_packets.erase(recv_header.sequence);

                //if it is acknowledge and syn we send the acknowledge and end the 3way handsake
                if (recv_header.flag == static_cast<int>(net_flag::NET_SYN_ACK) && recv_header.type == static_cast<int>(net_action::NET_CONECTION) && recv_header.sequence == 0)
                {
                    std::cout << "Received SYN-ACK: " << std::endl;
                    m_id = recv_header.id;
                    //sent the ack back
                    SendMsg(net_flag::NET_ACK, net_action::NET_CONECTION, m_id, 0, false);

                    //process the information of the rest of ships
                    NetMgr.AllShipsPacketProcess(recv_header, message, true);
                    
                    std::cout << "Sending ACK: " << std::endl;
                    std::cout << "Client Connected: " << std::endl;
                    return true;
                }
                return false;
            }
        }
        return false;
    }

    /**
    * this function will process a packet recieved and make different operations depending on the type of packet
    * @param packet         - all the information of the packet plus the data
    * @param data_length    - length of the packet
    * @return  void
    */
    bool client::ProcessPacket(std::vector<char> packet, int data_length)
    {
        //initialize variables to unpack the message
        net_header recv_header{};
        char msg[MAX_PAYLOAD_SIZE] = { 0 };
        current_alive_time = 0.0f;

        //get the info of the header and the message separated from the packet
        UnpackPacket(packet, data_length, recv_header, msg);

        //cast the info
        net_flag flag     = static_cast<net_flag>(recv_header.flag);
        net_action action = static_cast<net_action>(recv_header.type);

        //if the connection is ended
        if (flag == net_flag::NET_FIN && action == net_action::NET_CONECTION)
        {
            std::cout << "Server Disconected " << std::endl;
            return false;
        }
           
        //if the server didnt recieved the acknowledge of the syn ack connect we resend it
        if (action == net_action::NET_CONECTION && flag == net_flag::NET_SYN_ACK)
        {
            if (mbdebug)   std::cout << "Recieving SYN ACK ID : " << recv_header.sequence << std::endl;
            SendMsg(net_flag::NET_ACK, net_action::NET_CONECTION,m_id, 0, false);
        }

        //we recieved an acknowledge of a sended packet so we erase it from the map
        else if (flag == net_flag::NET_ACK)
        {
            if (mbdebug)   std::cout << "Recieving ACK ID : " << recv_header.sequence << std::endl;
            sended_packets.erase(recv_header.sequence);
        }
           
        //we recieved a proper packet of data so we need to check if its valid
        else if (flag == net_flag::NET_SEQ)
        {
            //if the message expect and acknowledge we send back an acknowledge of it
            if(recv_header.expect_ack)
                SendMsg(net_flag::NET_ACK, action, m_id, recv_header.sequence, false);

            //and process the packet
            NetMgr.ProcessPacket(recv_header, msg, data_length);
        }
        return true;
    }
}