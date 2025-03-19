/* Start Header
*****************************************************************/
/*!
\file client.cpp
\author 
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

SOCKET udpSocket;
enum class CMDID : uint8_t {
    UNKNOWN         = 0x00,
    REQ_QUIT        = 0x01,
    REQ_DOWNLOAD    = 0x02,
    RSP_DOWNLOAD    = 0x03,
    REQ_LISTFILES   = 0x04,
    RSP_LISTFILES   = 0x05,
    CMD_TEST        = 0x20,
    DOWNLOAD_ERROR  = 0x30
};

void recvTcpMsgs(int clientSocket);
void recvUdpMsgs(int clientUdpSocket, std::string filename, uint32_t expectedSessionID
    , sockaddr_in expectedServerAddr);
sockaddr_in udpAddr;
sockaddr_in serverUdpAddr;
std::string clientPath;


// This program requires one extra command-line parameter: a server hostname.
int main(int argc, char** argv)
{
    constexpr uint16_t port = 2048;

    // Get IP Address
    std::string host{};
    std::cout << "Server IP Address: ";
    std::getline(std::cin, host);

    // Get Port Number
    std::string portNumber;
    std::cout << "Server TCP Port Number: ";
    std::getline(std::cin, portNumber);


    std::string portString = portNumber;

    // Get Server UDP Port Number
    std::string serverUdpPort;
    std::cout << "Server UDP Port Number: ";
    std::getline(std::cin, serverUdpPort);

    // Get Client UDP Port Number
    std::string clientUdpPort;
    std::cout << "Client UDP Port Number: ";
    std::getline(std::cin, clientUdpPort);


    // Get Path for storing downloaded files
    std::cout << "Path: ";
    std::getline(std::cin, clientPath);


    // Convert port numbers to integers
    uint16_t serverUdpPortNum = static_cast<uint16_t>(std::stoi(serverUdpPort));
    uint16_t clientUdpPortNum = static_cast<uint16_t>(std::stoi(clientUdpPort));


    // This object holds the information about the version of Winsock that we
    // are using, which is not necessarily the version that we requested.
    WSADATA wsaData{};
    SecureZeroMemory(&wsaData, sizeof(wsaData));

    // Initialize Winsock. You must call WSACleanup when you are finished.
    // As this function uses a reference counter, for each call to WSAStartup,
    // you must call WSACleanup or suffer memory issues.
    int errorCode = WSAStartup(MAKEWORD(WINSOCK_VERSION, WINSOCK_SUBVERSION), &wsaData);
    if (NO_ERROR != errorCode)
    {
        std::cerr << "WSAStartup() failed." << std::endl;
        return errorCode;
    }

    // Object hints indicates which protocols to use to fill in the info.
    addrinfo hints{};
    SecureZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;			// IPv4
    hints.ai_socktype = SOCK_STREAM;	// Reliable delivery
    // Could be 0 to autodetect, but reliable delivery over IPv4 is always TCP.
    hints.ai_protocol = IPPROTO_TCP;	// TCP

    addrinfo* info = nullptr;
    errorCode = getaddrinfo(host.c_str(), portString.c_str(), &hints, &info);
    if ((NO_ERROR != errorCode) || (nullptr == info))
    {
        std::cerr << "getaddrinfo() failed." << std::endl;
        WSACleanup();
        return errorCode;
    }


    // Create UDP socket for receiving files
    udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udpSocket == INVALID_SOCKET) {
        std::cerr << "Failed to create UDP socket." << std::endl;
        WSACleanup();
        return 1;
    }

    // Bind the UDP socket to the specified client port
    udpAddr.sin_family = AF_INET;
    udpAddr.sin_addr.s_addr = INADDR_ANY; // Accept data from any address
    udpAddr.sin_port = htons(clientUdpPortNum);

    if (bind(udpSocket, (sockaddr*)&udpAddr, sizeof(udpAddr)) == SOCKET_ERROR) {
        std::cerr << "Failed to bind UDP socket." << std::endl;
        closesocket(udpSocket);
        WSACleanup();
        return 1;
    }

    
    serverUdpAddr.sin_family = AF_INET;
    serverUdpAddr.sin_port = htons(serverUdpPortNum); // The UDP port of the server
    inet_pton(AF_INET, host.c_str(), &serverUdpAddr.sin_addr);

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
    char bufferSend[MAX_STR_LEN] = {};


    bool sentTwice = false;

    std::thread recvThread(recvTcpMsgs, clientSocket);
    recvThread.detach();


    while (true)
    {
        //user input
        input.erase();
        std::getline(std::cin, input);
        
        //TCP Send
        if (input == "/q")
        {
            char byteSend = static_cast<char>(CMDID::REQ_QUIT);
            send(clientSocket, &byteSend, 1, 0);
            std::cout << "disconnection...\n";
            break;
        }
        else if (input.substr(0, 2) == "/l")
        {
            char byteSend = static_cast<char>(CMDID::REQ_LISTFILES);
            send(clientSocket, &byteSend, 1, 0);

        }
        else if (input.substr(0, 2) == "/d" && input.substr(2).size() > 0)
        {
            char bufferSend[MAX_STR_LEN] = {};
            int offset = 0;

            char cmdID = static_cast<char>(CMDID::REQ_DOWNLOAD);
            bufferSend[offset] = cmdID;
            offset += 1;

            //add clientIP to buffer
            size_t colonPos = input.find_first_of(':');
            std::string ipAddress = input.substr(3, colonPos - 3);
            in_addr clientIP;
            inet_pton(AF_INET, ipAddress.c_str(), &clientIP);
            memcpy(bufferSend + offset, &clientIP, sizeof(clientIP));
            offset += sizeof(clientIP);

            //add client PORT number to buffer
            size_t spacePos = input.find_first_of(' ', colonPos + 1);
            std::string portNumStr = input.substr(colonPos + 1, spacePos - colonPos - 1);
            uint16_t clientPortNum = htons(static_cast<uint16_t>(std::stoi(portNumStr)));
            memcpy(bufferSend + offset, &clientPortNum, sizeof(clientPortNum));
            offset += sizeof(clientPortNum);

            //extract filename
            std::string filename = input.substr(spacePos + 1);

            //add filename len to buffer
            uint32_t filenameLen = htonl(static_cast<uint32_t>(filename.size()));
            memcpy(bufferSend + offset, &filenameLen, sizeof(filenameLen));
            offset += sizeof(filenameLen);

            //add filename to buffer
            memcpy(bufferSend + offset, filename.data(), filename.size());
            offset += filename.size();

            int TCPbytesSent = send(clientSocket, bufferSend, offset, 0);
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

void recvUdpMsgs(int udpSocket, std::string fileName, uint32_t expectedSessionID
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
            udpSocket, bufferUdpRecv, sizeof(bufferUdpRecv) - 1, 0,
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
                sendto(udpSocket, bufferACKsend, sizeof(bufferACKsend), 0,
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

            sendto(udpSocket, bufferACKsend, sizeof(bufferACKsend), 0,
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


void recvTcpMsgs(int clientSocket)
{
    char bufferRecv[1000] = {};
    while (true)
    {   
        int receiveFromServer = recv(clientSocket, bufferRecv, MAX_STR_LEN, 0);


        //TCP RECV
        if (receiveFromServer > 0)
        {   
            bufferRecv[receiveFromServer] = '\0';
            int cmdID = static_cast<int>(bufferRecv[0]);

            if (cmdID == 3)
            {
                char serverIP[INET_ADDRSTRLEN];
                int offset = 1;

                //extract server IP address
                in_addr serverAddr;
                memcpy(&serverAddr, bufferRecv + offset, sizeof(serverAddr));
                inet_ntop(AF_INET, &serverAddr, serverIP, INET_ADDRSTRLEN);
                offset += sizeof(serverAddr);

                //extract server Port Num address
                uint16_t serverPortNum = ntohs(*(uint16_t*)(bufferRecv + offset));
                offset += 2;

                //extract session ID
                uint32_t sessionId = ntohl(*(uint32_t*)(bufferRecv + offset));
                offset += 4;

                //extract file len
                uint32_t fileSize = ntohl(*(uint32_t*)(bufferRecv + offset));
                offset += 4;

                uint32_t filenameLen = ntohl(*(uint32_t*)(bufferRecv + offset));
                offset += 4;

                std::string filename(bufferRecv + offset, filenameLen);
                offset += filenameLen;

                std::cout << "Server IP Address: " << serverIP << std::endl;
                std::cout << "Server UDP Port Number: " << serverPortNum << std::endl;
                std::cout << "Session ID: " << sessionId << std::endl;
                std::cout << "File Size: " << fileSize << std::endl;

                sockaddr_in expectedServerAddr;
                expectedServerAddr.sin_family = AF_INET;
                expectedServerAddr.sin_port = htons(serverPortNum);
                inet_pton(AF_INET, serverIP, &expectedServerAddr.sin_addr);

                std::thread UdprecvThread(recvUdpMsgs, udpSocket, filename, sessionId, expectedServerAddr);
                UdprecvThread.detach();
            }

            else if (cmdID == 5)
            {
                //extract RSP_LISTFILES datagram
                int offset = 1;

                //extract number of files
                uint16_t numFiles = ntohs(*(uint16_t*)(bufferRecv + offset));
                offset += 2;

                //extract length of file list
                uint32_t lenFileList = ntohl(*(uint32_t*)(bufferRecv + offset));
                offset += 4;

                std::cout << "Number of Files: " << numFiles << std::endl;
                std::cout << "Length of File List: " << lenFileList << std::endl;
                std::cout << "===============================" << std::endl;
                //extract file stats
                while (offset < receiveFromServer)
                {
                    uint32_t fileNameLen = ntohl(*(uint32_t*)(bufferRecv + offset));
                    offset += 4;
                    std::string fileName(bufferRecv + offset, fileNameLen);
                    offset += fileNameLen;
                    
                    std::cout << fileName << std::endl;
                }
            }

        }
        else
        {
            std::cout << "Server closed the connection.\n";
            break;
        }
    }
}