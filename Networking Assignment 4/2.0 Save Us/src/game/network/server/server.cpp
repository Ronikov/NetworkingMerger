/**
* @file server.cpp
* @author inigo fernandez , arenas.f , arenas.f@digipen.edu
* @date 2020/03/19
*
* This file contains the implementation of the server that will connect to the cient
*/

#include "server.hpp"
#include "game/network/system/networking.hpp"
#include "game/network/client/client.hpp"
#include <cassert>
#include <thread>
#include "game/game.hpp"

namespace network {
	
	/**
	* ShutDown of the server tht will close the socket and end the network connection, free winsock, it will also 
	* free all the data stored
	* @return  void
	*/
	void server::ShutDown()
	{
		//send the msg to disconec to all clients
		SendMsg(net_flag::NET_FIN, net_action::NET_CONECTION, 0, false);

		//release
		closesocket(m_socket);
		network_destroy();

		//clean the map and queue 
		sended_packets.clear();

		//delete all the clients
		for(auto& it : mClients)
			delete it.second;
	}

	/**
	* this function will start the server listening in the ip and port provided
	* @param ip			
	* @param port
	* @return  bool
	*/
	void server::Start(char const* ip, uint16_t port, bool debug)
	{
		//set debug mode
		mbdebug = debug;

		//initialize the winsock library
		network_create();
		
		//create socket and check that the creation didnt fail
		m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		assert(m_socket != INVALID_SOCKET);

		//remote endpoint
		sockaddr_in local_endpoint{};

		//convert IP from string to network address
		local_endpoint.sin_family = AF_INET;
		local_endpoint.sin_addr = cstr_to_ipv4(ip);
		local_endpoint.sin_port = htons(port);

		//make the socket non blocking
		u_long blocking_mode = 1;
		int err = ioctlsocket(m_socket, FIONBIO, &blocking_mode);
		assert(err == NO_ERROR);

		//bind the socket into the address
		err = bind(m_socket, reinterpret_cast<sockaddr*>(&local_endpoint), sizeof(local_endpoint));
		assert(err != SOCKET_ERROR);
	}

	/**
	* this function will update the server to handle new connection and then recieve packets and process that data to 
	* write into a file if the client is uploading or send new packets corresponding to a file if the client is downloading
	* @return  bool
	*/
	bool server::Update()
	{
		//integer for the recv
		int err = 1;
		//iterate until we read and process all the datagrams in the net
		while (err > 0)
		{
			//check for a new packet
			sockaddr_in	remote_endpoint = {};
			int remote_endpoint_size = sizeof(remote_endpoint);
			std::vector<char> recv_buffer;
			recv_buffer.resize(MAX_PAYLOAD_SIZE + sizeof(net_header));
			err = recvfrom(m_socket, recv_buffer.data(), static_cast<int>(recv_buffer.size()), 0, reinterpret_cast<sockaddr*>(&remote_endpoint), &remote_endpoint_size);

			//check for errors in recv
			if (err == -1)
			{
				//if there is no error
				err = WSAGetLastError();
				if (err != WSAEWOULDBLOCK)
					if(mbdebug)
						std::cout << "Error in recvfrom : " << err << std::endl;
				break;
			}

			//we recieved a packet so we need to process it
			else
			{
				//initialize variables to unpack the message
				net_header recv_header{};
				char msg[MAX_PAYLOAD_SIZE] = { 0 };

				//get the info of the header and the message separated from the packet
				UnpackPacket(recv_buffer, err, recv_header, msg);

				//cast the info
				net_flag flag = static_cast<net_flag>(recv_header.flag);
				net_action action = static_cast<net_action>(recv_header.type);

				if (action == net_action::NET_CONECTION)
					ConnectClient(recv_header, msg, remote_endpoint);
				else
				{
					auto it = mClients.find(recv_header.id);
					if (it == mClients.end()) return true;
					ProcessPacket(recv_header, msg, err - sizeof(net_header));
				}
			}
		}

		//update all the clients
		for (auto it : mClients)
		{
			//update the current client
			bool alive = it.second->Update();

			//if the client disconected
			if (!alive)
			{
				RemoveClient(it.first);
				break;
			}
		}
		return true;
	}

	//function to connect to a client
	void server::ConnectClient(net_header recv_header, char* msg, sockaddr_in const& _remote_address)
	{
		net_flag flag = static_cast<net_flag>(recv_header.flag);

		//if the connection is ended so we remove the client from every simulation
		if (flag == net_flag::NET_FIN)
			RemoveClient(recv_header.id);

		//if is a new conection request
		if (flag == net_flag::NET_SYN)
		{
			//check if the client was already allocated
			servers_client* new_client = CheckDuplicateClient(_remote_address);
			if (new_client == nullptr)
			{
				//create a new client
				new_client = new servers_client(_remote_address, m_socket, this);
				new_client->m_id = ++clients_count;
				mClients[new_client->m_id] = new_client;
			}

			//send the syn ack to the client with all the information of the clients and the new pos of the new client
			std::vector<char> msg = NetMgr.AllShipsPacketCreate(true, vec2(clients_count * 30, 0));
			new_client->SendMsg(net_flag::NET_SYN_ACK, net_action::NET_CONECTION, new_client->m_id, 0, true, msg.data(), msg.size());
		}
		else if (flag == net_flag::NET_ACK)
		{
			std::cout << "Client Connected ID: " << recv_header.id << std::endl;
			mClients[recv_header.id]->sended_packets.erase(recv_header.sequence);

			net_player new_player;
			new_player.id = recv_header.id;
			new_player.pos = vec2(recv_header.id * 10, 0);

			std::vector<char> new_player_data = NetMgr.CreateShip(new_player);
			SendMsg(net_flag::NET_SEQ, net_action::NET_PLAYER_NEW, recv_header.id, m_seq, true, new_player_data.data(), new_player_data.size());
		}
	}

	/**
	* this function will process a packet recieved and make different operations depending on the type of packet
	* @param packet         - all the information of the packet plus the data
	* @param data_length    - length of the packet
	* @return  void
	*/
	void server::ProcessPacket(net_header recv_header, char* msg, int data_length)
	{
		//cast the info
		net_flag flag = static_cast<net_flag>(recv_header.flag);
		net_action action = static_cast<net_action>(recv_header.type);
		mClients[recv_header.id]->current_alive_time = 0.0f;

		//we recieved an acknowledge of a sended packet so we erase it from the map
		if (flag == net_flag::NET_ACK)
		{
			if (mbdebug)   std::cout << "Recieving ACK ID : " << recv_header.sequence << std::endl;
			mClients[recv_header.id]->sended_packets.erase(recv_header.sequence);
		}

		//we recieved a proper packet of data 
		else if (flag == net_flag::NET_SEQ)
		{
			//if the message expect and acknowledge we send back an acknowledge of it
			if (recv_header.expect_ack)
				mClients[recv_header.id]->SendMsg(net_flag::NET_ACK, action, m_id, recv_header.sequence, false);

			//and then process the packet
			SendMsg(flag, action, recv_header.id, m_seq, false, msg, data_length);
			NetMgr.ProcessPacket(recv_header, msg, data_length);
		}
	}

	void server::SendMsg(net_flag flag, net_action action, int id, int seq_num, bool expected_acknowledge, const char* msg, int size)
	{
		for (auto& it : mClients)
			if(id == 0 || id != it.first)
				it.second->SendMsg(flag, action, id, ++it.second->m_seq, expected_acknowledge, msg, size);
	}

	servers_client* server::CheckDuplicateClient(sockaddr_in const& _remote_address)
	{
		for (auto& it : mClients)
		{
			//check if the client was already connected to the server
			sockaddr_in curr_endpoint = it.second->m_remote_endpoint;
			if (ipv4_to_str(_remote_address.sin_addr)  == ipv4_to_str(_remote_address.sin_addr) &&
				_remote_address.sin_family == curr_endpoint.sin_family &&
				_remote_address.sin_port == curr_endpoint.sin_port)
				return it.second;
		}
		return nullptr;
	}

	void server::RemoveClient(int client_id)
	{
		//erase the disconected client
		auto cl_it = mClients.find(client_id);
		if (cl_it == mClients.end()) return;

		//remove the player in the game logic
		NetMgr.RemovePlayer(client_id);
		servers_client* temp_cl = cl_it->second;
		mClients.erase(cl_it);

		//delete the client
		delete temp_cl;

		//remove the  client from the rest of simulations
		SendMsg(net_flag::NET_SEQ, net_action::NET_PLAYER_DISCONECTS, client_id, 0, true);
		std::cout << "Client Disconected" << std::endl;
	}

	servers_client::servers_client(sockaddr_in const& _remote_address, SOCKET s, server* server)
	{
		m_remote_endpoint = _remote_address;
		m_socket = s;
		mServer = server;
	}

	bool servers_client::Update()
	{
		UpdateSendedPackets();

		//check if the client didnt send any message for a long period of time so we assume that the 
		// cliet disconected, then we reset the client and wait for a new one
		if (current_alive_time > alive_timer.count())
			return false;
		return true;
	}
}
