#include "utilsnetwork.hpp"
#include <iostream>
#include <cstring>
#include <cassert>
#include <signal.h>

#ifdef WIN32
void network_create()
{
	//initialize winsock library
	WSAData data{};
	auto err = WSAStartup(MAKEWORD(2, 2), &data);

	//check everything went good
	assert(err == 0);
	assert(LOBYTE(data.wVersion) == 2);
	assert(HIBYTE(data.wVersion) == 2);
}

void network_destroy()
{
	//release
	WSACleanup();
}


/**
 * @brief
 *  Writes an address with the format {ip}[:port]. Port is not written if 0
 * @param src
 * @return sockaddr_in
 */
std::ostream& operator<<(std::ostream& os, sockaddr_in const& addr)
{
	//put the address and the port in os
	os << addr.sin_addr;
	os << ":";
	os << ntohs(addr.sin_port);
	return os;
}

std::ostream& operator<<(std::ostream& os, in_addr const& ipv4)
{
	//get the ip and put in os
	char remote_address[4096]{};
	inet_ntop(AF_INET, &ipv4, remote_address, sizeof(remote_address));
	os << remote_address;
	return os;
}

/**
 * @brief
 *  Converts a c-string into an ip address. Throws if fails.
 * @param str
 * @return in_addr
 */
in_addr cstr_to_ipv4(char const* str)
{
	in_addr address = {};
	int err = inet_pton(AF_INET, str, &address);
	assert(err == 1);
	return address;
}

/**
 * @brief
 *  Converts an address to an std::string
 * @param addr
 * @return std::string
 */
std::string ipv4_to_str(in_addr const& addr)
{
	char remote_address[4096]{};
	inet_ntop(AF_INET, &addr, remote_address, sizeof(remote_address));
	return std::string(remote_address);
}



#else

#endif
