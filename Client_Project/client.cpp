/* Start Header
*****************************************************************/
/*!
\file client.cpp
\author Zulfami, b.zulfamiashrafi@digipen.edu
\co author Brandon Poon, b.poon@digipen.edu
\co author Gabriel Peh, peh.j@digipen.edu
\par
\date 17 March 2025
\brief
Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the

prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
*******************************************************************/

/*******************************************************************************
 * A simple TCP/IP client application
 ******************************************************************************/

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "Windows.h"		// Entire Win32 API...
#include "winsock2.h"		// ...or Winsock alone
#include "ws2tcpip.h"		// getaddrinfo()

 // Tell the Visual Studio linker to include the following library in linking.
 // Alternatively, we could add this file to the linker command-line parameters,
 // but including it in the source code simplifies the configuration.
#pragma comment(lib, "ws2_32.lib")

#include <iostream>			// cout, cerr
#include <string>			// string
#include <vector>
#include <thread>
#include <fstream>
#include <unordered_set>
#include <filesystem>

#define WINSOCK_VERSION     2
#define WINSOCK_SUBVERSION  2
#define MAX_STR_LEN         1000
#define RETURN_CODE_1       1
#define RETURN_CODE_2       2
#define RETURN_CODE_3       3
#define RETURN_CODE_4       4

SOCKET UDP_SOCKET;
enum CMDID {
    UNKNOWN = (unsigned char)0x0,
    REQ_QUIT = (unsigned char)0x1,
    REQ_DOWNLOAD = (unsigned char)0x2,
    RSP_DOWNLOAD = (unsigned char)0x3,
    REQ_LISTFILES = (unsigned char)0x4,
    RSP_LISTFILES = (unsigned char)0x5,
    CMD_TEST = (unsigned char)0x20,
    DOWNLOAD_ERROR = (unsigned char)0x30
};

void recv_TCP(int clientSocket);
void recv_UDP(int clientUDP_SOCKET, std::string filename, uint32_t expectedSessionID, sockaddr_in expectedServerAddr);
sockaddr_in UDP_ADDR;
sockaddr_in SERVER_UDP_ADDR;
std::string clientPath;


// This program requires one extra command-line parameter: a server hostname.
int main(int argc, char** argv)
{
    constexpr uint16_t port = 2048;

    std::string host, portNumber, portString, serverUdpPort, clientUdpPort;

    std::cout << "Server IP Address: ";
    std::getline(std::cin, host);

    std::cout << "Server TCP Port Number: ";
    std::getline(std::cin, portNumber);


    portString = portNumber;

    std::cout << "Server UDP Port Number: ";
    std::getline(std::cin, serverUdpPort);

    std::cout << "Client UDP Port Number: ";
    std::getline(std::cin, clientUdpPort);

    std::cout << "Path: ";
    std::getline(std::cin, clientPath);

    uint16_t server_UDP_Port = static_cast<uint16_t>(std::stoi(serverUdpPort));
    uint16_t client_UDP_Port = static_cast<uint16_t>(std::stoi(clientUdpPort));

    WSADATA wsaData{};
    SecureZeroMemory(&wsaData, sizeof(wsaData));

    int errorCode = WSAStartup(MAKEWORD(WINSOCK_VERSION, WINSOCK_SUBVERSION), &wsaData);
    if (NO_ERROR != errorCode)
    {
        std::cerr << "WSAStartup() failed." << std::endl;
        return errorCode;
    }

    addrinfo hints{};
    SecureZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;			
    hints.ai_socktype = SOCK_STREAM;	
    hints.ai_protocol = IPPROTO_TCP;	

    addrinfo* info = nullptr;
    errorCode = getaddrinfo(host.c_str(), portString.c_str(), &hints, &info);
    if ((NO_ERROR != errorCode) || (nullptr == info))
    {
        std::cerr << "getaddrinfo() failed." << std::endl;
        WSACleanup();
        return errorCode;
    }


    UDP_SOCKET = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (UDP_SOCKET == INVALID_SOCKET) {
        std::cerr << "UDO socket invalid." << std::endl;
        WSACleanup();
        return 1;
    }

    UDP_ADDR.sin_family = AF_INET;
    UDP_ADDR.sin_addr.s_addr = INADDR_ANY; 
    UDP_ADDR.sin_port = htons(client_UDP_Port);

    if (bind(UDP_SOCKET, (sockaddr*)&UDP_ADDR, sizeof(UDP_ADDR)) == SOCKET_ERROR) {
        std::cerr << "UDP socket bind failed." << std::endl;
        closesocket(UDP_SOCKET);
        WSACleanup();
        return 1;
    }

    
    SERVER_UDP_ADDR.sin_family = AF_INET;
    SERVER_UDP_ADDR.sin_port = htons(server_UDP_Port); 
    inet_pton(AF_INET, host.c_str(), &SERVER_UDP_ADDR.sin_addr);

    SOCKET clientSocket = socket(
        info->ai_family,
        info->ai_socktype,
        info->ai_protocol);
    if (INVALID_SOCKET == clientSocket)
    {
        std::cerr << "socket() failed." << std::endl;
        freeaddrinfo(info);
        WSACleanup();
        return RETURN_CODE_2;
    }

    errorCode = connect(
        clientSocket,
        info->ai_addr,
        static_cast<int>(info->ai_addrlen));
    if (SOCKET_ERROR == errorCode)
    {
        std::cerr << "connect() failed." << std::endl;
        freeaddrinfo(info);
        closesocket(clientSocket);
        WSACleanup();
        return RETURN_CODE_3;
    }

    std::string input, inputSent;
    char buffer[MAX_STR_LEN] = {};

    std::thread recvThread(recv_TCP, clientSocket);
    recvThread.detach();

    while (true)
    {
        input.erase();
        std::getline(std::cin, input);
        
        char cmd = input[1];
        char slash = input[0];
        int offset = 0;
        int cmd_offset = 3;
        if (slash == '/')
        {
            char message;
            switch (cmd)
            {
            case 'q':
                message = CMDID::REQ_QUIT;
                send(clientSocket, &message, sizeof(message), 0);
                std::cout << "disconnection...\n";
                break;
            case 'l':
                message = CMDID::REQ_LISTFILES;
                send(clientSocket, &message, sizeof(message), 0);
                break;
            case 'd':
                std::vector<char> buffer;
                buffer.resize(MAX_STR_LEN); 

                size_t offset = 0;

                char cmdID = CMDID::REQ_DOWNLOAD;
                buffer[offset] = cmdID;
                offset += 1;

                size_t ipEndPos = input.find(':');
                std::string ipAddress = input.substr(cmd_offset, ipEndPos - cmd_offset);

                in_addr clientIPAddress{}; 
                inet_pton(AF_INET, ipAddress.c_str(), &clientIPAddress);

                buffer.insert(buffer.begin() + offset, reinterpret_cast<char*>(&clientIPAddress), reinterpret_cast<char*>(&clientIPAddress) + sizeof(clientIPAddress));
                offset += sizeof(clientIPAddress);

                size_t spacePos = input.find_last_of(' ');
                std::string portString = input.substr(ipEndPos + 1, spacePos - ipEndPos - 1);
                uint16_t clientPort = htons(static_cast<uint16_t>(std::stoi(portString)));

                buffer.insert(buffer.begin() + offset, reinterpret_cast<char*>(&clientPort), reinterpret_cast<char*>(&clientPort) + sizeof(clientPort));
                offset += sizeof(clientPort);

                std::string filename = input.substr(spacePos + 1);

                uint32_t filenameLength = htonl(static_cast<uint32_t>(filename.size()));
                buffer.insert(buffer.begin() + offset, reinterpret_cast<char*>(&filenameLength), reinterpret_cast<char*>(&filenameLength) + sizeof(filenameLength));
                offset += sizeof(filenameLength);

                buffer.insert(buffer.begin() + offset, filename.begin(), filename.end());
                offset += filename.size();

                send(clientSocket, buffer.data(), offset, 0);
                break;
            }
        }
    }
    errorCode = shutdown(clientSocket, SD_SEND);
    if (SOCKET_ERROR == errorCode)
    {
        std::cerr << "shutdown() failed." << std::endl;
    }
    closesocket(clientSocket);
    WSACleanup();
}



void recv_UDP(int UDP_SOCKET, std::string fileName, uint32_t expectedSessionID
    , sockaddr_in expectedServerAddr)
{   
    sockaddr_in serverAddr, serverAddrRecv;
    int serverAddrSize = sizeof(serverAddr);
    char bufferUdpRecv[2000] = {};
    char bufferACKsend[4] = {};

    std::string filePath = clientPath + "/" + fileName;
    std::ofstream outFile(filePath, std::ios::binary);
    if (!outFile)
    {
        std::cerr << "Error: Cannot create or write to file " << filePath << std::endl;
        return;
    }
    
    uint32_t expectedSeqNum = 1;
    bool completedTransfer = false;
    std::unordered_set<uint32_t> receivedSeqNums; // Track received packets
    uint32_t totalDataRecv = 0;

    while (!completedTransfer)
    {   
        int recvUDPfromServer = recvfrom(
            UDP_SOCKET, bufferUdpRecv, sizeof(bufferUdpRecv) - 1, 0,
            (sockaddr*)&serverAddr, &serverAddrSize
        );


        int offset = 0;

        //UDP RECV
        if (recvUDPfromServer == -1) {
            std::cerr << "[UDP] Error receiving data! Errno: " << WSAGetLastError() << std::endl;
        }
        else if (recvUDPfromServer > 0)
        {   

            if (serverAddr.sin_addr.s_addr != expectedServerAddr.sin_addr.s_addr ||
                serverAddr.sin_port != expectedServerAddr.sin_port)
            {
                std::cout << "[UDP] Ignoring packet from unknown server" << std::endl;
                continue;
            }

            //extract sessionID
            uint32_t sessionID = ntohl(*(uint32_t*)(bufferUdpRecv + offset));
            offset += 4;

            if (sessionID != expectedSessionID) {
                std::cout << "[UDP] Ignoring packet: Incorrect session ID " << sessionID
                    << " (Expected: " << expectedSessionID << ")" << std::endl;
                continue;
            }

            //extract fileSize
            uint32_t fileSize = ntohl(*(uint32_t*)(bufferUdpRecv + offset));
            offset += 4;

            //extract file offset
            uint32_t fileOffset = ntohl(*(uint32_t*)(bufferUdpRecv + offset));
            offset += 4;

            //extract file data size
            uint32_t fileChunkSize = ntohl(*(uint32_t*)(bufferUdpRecv + offset));
            offset += 4;

            //extract seqNum
            uint32_t seqNum = ntohl(*(uint32_t*)(bufferUdpRecv + offset));
            offset += 4;

            if (seqNum == expectedSeqNum && receivedSeqNums.find(seqNum) == receivedSeqNums.end())
            {   
                outFile.seekp(fileOffset);
                outFile.write(bufferUdpRecv + offset, fileChunkSize);
                receivedSeqNums.insert(seqNum);

                expectedSeqNum++;
                totalDataRecv += fileChunkSize;
                std::cout << "Data Chunk Sent\n";
            }
            else if (seqNum < expectedSeqNum)
            {
                std::cout << "[UDP] Duplicate packet received! SeqNum: " << seqNum << std::endl;
            }
            else if (seqNum > expectedSeqNum)
            {
                std::cout << "[UDP] Future packet received! Expected: " << expectedSeqNum
                    << ", Got: " << seqNum << " (Packet lost?)" << std::endl;

                //Ask the server to resend missing packet
                uint32_t ackNumNet = htonl(expectedSeqNum - 1);
                memcpy(bufferACKsend, &ackNumNet, sizeof(ackNumNet));
                sendto(UDP_SOCKET, bufferACKsend, sizeof(bufferACKsend), 0,
                    (sockaddr*)&serverAddrRecv, sizeof(serverAddrRecv));

                std::cout << "[UDP] Requesting Resend for SeqNum: " << expectedSeqNum << std::endl;
                continue;
            }
            else
            {
                std::cout << "[UDP] Out-of-order packet received! Expected: "
                    << expectedSeqNum << ", Got: " << seqNum << std::endl;
            }

            //send ack packet
            uint32_t acknum = seqNum;
            uint32_t ackNumNet = htonl(acknum);
            memcpy(bufferACKsend, &ackNumNet, sizeof(ackNumNet));


            memcpy(&serverAddrRecv, &serverAddr, sizeof(serverAddr));

            sendto(UDP_SOCKET, bufferACKsend, sizeof(bufferACKsend), 0,
                (sockaddr*)&serverAddrRecv, sizeof(serverAddrRecv));
            std::cout << "ACK Sent for SeqNum: " << seqNum << std::endl;

            if (totalDataRecv >= fileSize)
            {
                completedTransfer = true;
                break;
            }

            
        }
        else
        {
            std::cout << "Not Receiving Bytes\n";
        }

    }
    outFile.close();
    std::cout << "[UDP] File received successfully!" << std::endl;
}




void handleDownloadResponse(const char* bufferRecv, int bytesReceived)
{
    int offset = 1;
    in_addr serverAddr;
    memcpy(&serverAddr, bufferRecv + offset, sizeof(serverAddr));
    char serverIP[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &serverAddr, serverIP, INET_ADDRSTRLEN);
    offset += sizeof(serverAddr);

    uint16_t serverPortNum = ntohs(*(uint16_t*)(bufferRecv + offset));
    offset += sizeof(uint16_t);

    uint32_t sessionId = ntohl(*(uint32_t*)(bufferRecv + offset));
    offset += sizeof(uint32_t);

    uint32_t fileSize = ntohl(*(uint32_t*)(bufferRecv + offset));
    offset += sizeof(uint32_t);

    uint32_t filenameLen = ntohl(*(uint32_t*)(bufferRecv + offset));
    offset += sizeof(uint32_t);

    std::string filename(bufferRecv + offset, filenameLen);

    std::cout << "Server IP Address: " << serverIP << "\n"
        << "Server UDP Port Number: " << serverPortNum << "\n"
        << "Session ID: " << sessionId << "\n"
        << "File Size: " << fileSize << "\n";

    sockaddr_in expectedServerAddr{};
    expectedServerAddr.sin_family = AF_INET;
    expectedServerAddr.sin_port = htons(serverPortNum);
    inet_pton(AF_INET, serverIP, &expectedServerAddr.sin_addr);

    std::thread udpRecvThread(recv_UDP, UDP_SOCKET, filename, sessionId, expectedServerAddr);
    udpRecvThread.detach();
}

void handleListFilesResponse(const char* bufferRecv, int bytesReceived)
{
    int offset = 1;
    uint16_t numFiles = ntohs(*(uint16_t*)(bufferRecv + offset));
    offset += sizeof(uint16_t);

    uint32_t lenFileList = ntohl(*(uint32_t*)(bufferRecv + offset));
    offset += sizeof(uint32_t);

    std::cout << "Number of Files: " << numFiles << "\n"
        << "Length of File List: " << lenFileList << "\n"
        << "===============================\n";

    while (offset < bytesReceived)
    {
        uint32_t fileNameLen = ntohl(*(uint32_t*)(bufferRecv + offset));
        offset += sizeof(uint32_t);

        std::string fileName(bufferRecv + offset, fileNameLen);
        offset += fileNameLen;

        std::cout << fileName << "\n";
    }
}

void recv_TCP(int clientSocket)
{
    while (true)
    {
        char bufferRecv[MAX_STR_LEN] = {};
        int bytesReceived = recv(clientSocket, bufferRecv, MAX_STR_LEN, 0);

        if (bytesReceived <= 0)
        {
            std::cout << "Server closed the connection.\n";
            break;
        }

        bufferRecv[bytesReceived] = '\0';
        char cmdID = bufferRecv[0];

        switch (cmdID)
        {
        case RSP_DOWNLOAD:
            handleDownloadResponse(bufferRecv, bytesReceived);
            break;
        case RSP_LISTFILES:
            handleListFilesResponse(bufferRecv, bytesReceived);
            break;
        default:
            std::cerr << "Unknown command received: " << static_cast<int>(cmdID) << "\n";
            break;
        }
    }
}