/* Start Header
*****************************************************************/
/*!
\file client.cpp
\author
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
 * A multi-threaded TCP/IP server application
 ******************************************************************************/

 #ifndef WIN32_LEAN_AND_MEAN
 #define WIN32_LEAN_AND_MEAN
 #endif
 
 #include "Windows.h"		// Entire Win32 API...
  // #include "winsock2.h"	// ...or Winsock alone
 #include "ws2tcpip.h"		// getaddrinfo()
 
 // Tell the Visual Studio linker to include the following library in linking.
 // Alternatively, we could add this file to the linker command-line parameters,
 // but including it in the source code simplifies the configuration.
 #pragma comment(lib, "ws2_32.lib")
 
#include <iostream>			// cout, cerr
#include <string>			// string
#include <map>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <unordered_map>
#include <queue>
 
#pragma region TASKQUEUE.H
template <typename TItem, typename TAction, typename TOnDisconnect>
class TaskQueue
{
public:
    TaskQueue(size_t workerCount, size_t slotCount, TAction& action, TOnDisconnect& disconnect);
    ~TaskQueue();

    std::optional<TItem> consume();
    void produce(TItem item);

    TaskQueue() = delete;
    TaskQueue(const TaskQueue&) = delete;
    TaskQueue(TaskQueue&&) = delete;
    TaskQueue& operator=(const TaskQueue&) = delete;
    TaskQueue& operator=(TaskQueue&&) = delete;

private:

    static void work(TaskQueue<TItem, TAction, TOnDisconnect>& tq, TAction& action);
    void disconnect();

    // Pool of worker threads.
    std::vector<std::thread> _workers;

    // Buffer of slots for items.
    std::mutex _bufferMutex;
    std::queue<TItem> _buffer;

    // Count of available slots.
    std::mutex _slotCountMutex;
    size_t _slotCount;
    // Critical section condition for decreasing slots.
    std::condition_variable _producers;

    // Count of available items.
    std::mutex _itemCountMutex;
    size_t _itemCount;
    // Critical section condition for decreasing items.
    std::condition_variable _consumers;

    volatile bool _stay;

    TOnDisconnect& _onDisconnect;
};
#pragma endregion

#pragma region TASKQUEUE.HPP

static std::mutex _stdoutMutex;
template <typename TItem, typename TAction, typename TOnDisconnect>
TaskQueue<TItem, TAction, TOnDisconnect>::TaskQueue(size_t workerCount, size_t slotCount, TAction& action, TOnDisconnect& onDisconnect) :
    _slotCount{ slotCount },
    _itemCount{ 0 },
    _onDisconnect{ onDisconnect },
    _stay{ true }
{
    for (size_t i = 0; i < workerCount; ++i)
    {
        _workers.emplace_back(&work, std::ref(*this), std::ref(action));
    }
}

template <typename TItem, typename TAction, typename TOnDisconnect>
void TaskQueue<TItem, TAction, TOnDisconnect>::produce(TItem item)
{
    // Non-RAII unique_lock to be blocked by a producer who needs a slot.
    {
        // Wait for an available slot...
        std::unique_lock<std::mutex> slotCountLock{ _slotCountMutex };
        _producers.wait(slotCountLock, [&]() { return _slotCount > 0; });
        --_slotCount;
    }
    // RAII lock_guard locked for buffer.
    {
        // Lock the buffer.
        std::lock_guard<std::mutex> bufferLock{ _bufferMutex };
        _buffer.push(item);
    }
    // RAII lock_guard locked for itemCount.
    {
        // Announce available item.
        std::lock_guard<std::mutex> itemCountLock(_itemCountMutex);
        ++_itemCount;
        _consumers.notify_one();
    }
}

template <typename TItem, typename TAction, typename TOnDisconnect>
std::optional<TItem> TaskQueue<TItem, TAction, TOnDisconnect>::consume()
{
    std::optional<TItem> result = std::nullopt;
    // Non-RAII unique_lock to be blocked by a consumer who needs an item.
    {
        // Wait for an available item or termination...
        std::unique_lock<std::mutex> itemCountLock{ _itemCountMutex };
        _consumers.wait(itemCountLock, [&]() { return (_itemCount > 0) || (!_stay); });
        if (_itemCount == 0)
        {
            _consumers.notify_one();
            return result;
        }
        --_itemCount;
    }
    // RAII lock_guard locked for buffer.
    {
        // Lock the buffer.
        std::lock_guard<std::mutex> bufferLock{ _bufferMutex };
        result = _buffer.front();
        _buffer.pop();
    }
    // RAII lock_guard locked for slots.
    {
        // Announce available slot.
        std::lock_guard<std::mutex> slotCountLock{ _slotCountMutex };
        ++_slotCount;
        _producers.notify_one();
    }
    return result;
}

template <typename TItem, typename TAction, typename TOnDisconnect>
void TaskQueue<TItem, TAction, TOnDisconnect>::work(TaskQueue<TItem, TAction, TOnDisconnect>& tq, TAction& action)
{
    while (true)
    {
        {
            std::lock_guard<std::mutex> usersLock{ _stdoutMutex };
            std::cout
                << "Thread ["
                << std::this_thread::get_id()
                << "] is waiting for a task."
                << std::endl;
        }
        std::optional<TItem> item = tq.consume();
        if (!item)
        {
            // Termination of idle threads.
            break;
        }

        {
            std::lock_guard<std::mutex> usersLock{ _stdoutMutex };
            std::cout
                << "Thread ["
                << std::this_thread::get_id()
                << "] is executing a task."
                << std::endl;
        }

        if (!action(*item))
        {
            // Decision to terminate workers.
            tq.disconnect();
        }
    }

    {
        std::lock_guard<std::mutex> usersLock{ _stdoutMutex };
        std::cout
            << "Thread ["
            << std::this_thread::get_id()
            << "] is exiting."
            << std::endl;
    }
}

template <typename TItem, typename TAction, typename TOnDisconnect>
void TaskQueue<TItem, TAction, TOnDisconnect>::disconnect()
{
    _stay = false;
    _onDisconnect();
}

template <typename TItem, typename TAction, typename TOnDisconnect>
TaskQueue<TItem, TAction, TOnDisconnect>::~TaskQueue()
{
    disconnect();
    for (std::thread& worker : _workers)
    {
        worker.join();
    }
}
#pragma endregion
 
 bool execute(SOCKET clientSocket);
 void disconnect(SOCKET& tcpSocket);
 void handleListFiles(SOCKET clientSocket);
 void handleFileDownload(SOCKET clientSocket, const char* buffer);
 void sendFileToClient(int udpSocket, uint32_t sessionID,
     std::string filename, uint16_t clientPortNum, std::string clientIP, uint32_t sessionIDNetwork,
     uint32_t fileSizeNet, std::string filePath);

 SOCKET udpSocket;

#define BUFFER_SIZE         1000
#define CHUNK_SIZE          1024
#define TIMEOUT_MS          2000

 using FileList = std::vector<std::pair<std::string, uint32_t>>;
 using SessionMap = std::unordered_map<uint32_t, uint32_t>;
 using ClientMap = std::map<SOCKET, std::pair<std::string, uint16_t>>;
 
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

ClientMap       connected_Clients;
std::string     path;
char            serverIPAddress[BUFFER_SIZE];
std::string     portNumber_UDP;
static uint32_t sessionCounter = 1;
sockaddr_in     udpAddress;
FileList        fileList;
SessionMap      sessionSeqNum;
 
 int main()
 {
     // Get Port Number
     std::string TCPportNumber;
     std::cout << "Server TCP Port Number: ";
     std::getline(std::cin, TCPportNumber);
     std::cout << "Server UDP Port Number: ";
     std::getline(std::cin, portNumber_UDP);
     std::cout << "Path: ";
     std::getline(std::cin, path);
 
     std::string TCPportString = TCPportNumber;
     uint16_t udpPort = static_cast<uint16_t>(std::stoi(portNumber_UDP));
 
     WSADATA wsaData{};

     int errorCode = WSAStartup(MAKEWORD(2, 2), &wsaData);
     if (errorCode != NO_ERROR)
     {
         std::cerr << "WSAStartup() failed." << std::endl;
         return errorCode;
     }

     addrinfo hints{};
     SecureZeroMemory(&hints, sizeof(hints));
     hints.ai_family = AF_INET;
     hints.ai_socktype = SOCK_STREAM;
     hints.ai_protocol = IPPROTO_TCP;
     hints.ai_flags = AI_PASSIVE;
 
     char host[BUFFER_SIZE];
     gethostname(host, BUFFER_SIZE);
 
     addrinfo* info = nullptr;
     errorCode = getaddrinfo(host, TCPportString.c_str(), &hints, &info);
     if ((errorCode) || (info == nullptr))
     {
         std::cerr << "getaddrinfo() failed." << std::endl;
         WSACleanup();
         return errorCode;
     }
 
     struct sockaddr_in* serverAddress = reinterpret_cast<struct sockaddr_in*> (info->ai_addr);
     inet_ntop(AF_INET, &(serverAddress->sin_addr), serverIPAddress, INET_ADDRSTRLEN);
     getnameinfo(info->ai_addr, static_cast <socklen_t> (info->ai_addrlen), serverIPAddress, sizeof(serverIPAddress), nullptr, 0, NI_NUMERICHOST);
     std::cout << std::endl;
     std::cout << "Server IP Address: " << serverIPAddress << std::endl;
     std::cout << "Server TCP Port Number: " << TCPportString << std::endl;
     std::cout << "Server UDP Port Number: " << udpPort << std::endl;
 
 
     SOCKET tcpSocket = socket(
         hints.ai_family,
         hints.ai_socktype,
         hints.ai_protocol);

     if (tcpSocket == INVALID_SOCKET)
     {
         std::cerr << "TCPsocket() failed." << std::endl;
         freeaddrinfo(info);
         WSACleanup();
         return 1;
     }
 
     errorCode = bind(
         tcpSocket,
         info->ai_addr,
         static_cast<int>(info->ai_addrlen));
     if (errorCode != NO_ERROR)
     {
         std::cerr << "TCPbind() failed." << std::endl;
         closesocket(tcpSocket);
         tcpSocket = INVALID_SOCKET;
     }
 
     freeaddrinfo(info);
 
     if (tcpSocket == INVALID_SOCKET)
     {
         std::cerr << "TCPbind() failed." << std::endl;
         WSACleanup();
         return 2;
     }

     udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
     if (udpSocket == INVALID_SOCKET) {
         std::cerr << "Failed to create UDP socket." << std::endl;
         WSACleanup();
         return 1;
     }

     udpAddress.sin_family = AF_INET;
     udpAddress.sin_addr.s_addr = INADDR_ANY;
     udpAddress.sin_port = htons(udpPort);

     if (bind(udpSocket, (sockaddr*)&udpAddress, sizeof(udpAddress)) == SOCKET_ERROR) {
         std::cerr << "Failed to bind UDP socket." << std::endl;
         closesocket(udpSocket);
         WSACleanup();
         return 1;
     }
 
 
     errorCode = listen(tcpSocket, SOMAXCONN);
     if (errorCode != NO_ERROR)
     {
         std::cerr << "TCPlisten() failed." << std::endl;
         closesocket(tcpSocket);
         WSACleanup();
         return 3;
     }

 
     {
         const auto onDisconnect = [&]() { disconnect(tcpSocket); };
         auto tq = TaskQueue<SOCKET, decltype(execute), decltype(onDisconnect)>{10, 20, execute, onDisconnect};
         while (tcpSocket != INVALID_SOCKET)
         {
             sockaddr clientAddress{};
             SecureZeroMemory(&clientAddress, sizeof(clientAddress));
             int clientAddressSize = sizeof(clientAddress);
             SOCKET clientSocket = accept(
                 tcpSocket,
                 &clientAddress,
                 &clientAddressSize);
             if (clientSocket == INVALID_SOCKET)
             {
                 break;
             }
             sockaddr_in* clientAddrIn = reinterpret_cast<sockaddr_in*>(&clientAddress);
             char clientIP[BUFFER_SIZE];
             inet_ntop(AF_INET, &clientAddrIn->sin_addr, clientIP, INET_ADDRSTRLEN);
             int clientPort = ntohs(clientAddrIn->sin_port);
 
             std::cout << '\n' << "Client IP Address: " << clientIP << '\n'
                 << "Client Port Number: " << clientPort << '\n';
 
             tq.produce(clientSocket);
             connected_Clients[clientSocket] = { clientIP, clientPort };
         }
     }
 
     WSACleanup();
 }
 
 void disconnect(SOCKET& tcpSocket)
 {
     if (tcpSocket != INVALID_SOCKET)
     {
         shutdown(tcpSocket, SD_BOTH);
         closesocket(tcpSocket);
         tcpSocket = INVALID_SOCKET;
     }
 }

 FileList GetFiles(std::string path)
{
     std::vector<std::pair<std::string, uint32_t>> fileList;
    std::filesystem::path currPath{ path };

    for (auto const& dir : std::filesystem::directory_iterator{ currPath })
    {
        std::string filename = dir.path().filename().string();
        uint32_t filenameLen = static_cast<uint32_t>(filename.size());

        fileList.push_back({ filename, filenameLen });
    }

    return fileList;
}
 
 bool execute(SOCKET clientSocket)
 {
     char buffer[BUFFER_SIZE];
     bool stay = true;

     sockaddr_in clientAddr;
     int clientAddrSize = sizeof(clientAddr);

     while (true)
     {
         int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);
         if (bytesReceived <= 0)
         {
             std::cerr << (bytesReceived == 0 ? "Graceful shutdown." : "recv() failed.") << std::endl;
             break;
         }
         buffer[bytesReceived] = { 0 };
         char cmdID = buffer[0];

         switch (cmdID)
         {
         case REQ_LISTFILES:
             handleListFiles(clientSocket);
             break;
         case REQ_DOWNLOAD:
             handleFileDownload(clientSocket, buffer + 1);
             break;
         default:
             std::cerr << "Invalid command received: " << cmdID << std::endl;
             break;
         }
     }

     shutdown(clientSocket, SD_BOTH);
     closesocket(clientSocket);
     return stay;
 }

 void handleListFiles(SOCKET clientSocket)
 {
     char bufferSend[1000] = {};
     int offset = 0;
     bufferSend[offset++] = static_cast<char>(CMDID::RSP_LISTFILES);

     auto fileList = GetFiles(path);
     uint16_t numFilesNet = htons(static_cast<uint16_t>(fileList.size()));
     memcpy(bufferSend + offset, &numFilesNet, sizeof(numFilesNet));
     offset += sizeof(numFilesNet);

     size_t lengthFileList = 4 * fileList.size();
     for (const auto& file : fileList)
         lengthFileList += file.second;

     uint32_t lenFileListNet = htonl(static_cast<uint32_t>(lengthFileList));
     memcpy(bufferSend + offset, &lenFileListNet, sizeof(lenFileListNet));
     offset += sizeof(lenFileListNet);

     for (const auto& file : fileList)
     {
         uint32_t fileNameLenNet = htonl(static_cast<uint32_t>(file.second));
         memcpy(bufferSend + offset, &fileNameLenNet, sizeof(fileNameLenNet));
         offset += sizeof(fileNameLenNet);

         memcpy(bufferSend + offset, file.first.c_str(), file.first.size());
         offset += (int)file.first.size();
     }

     send(clientSocket, bufferSend, offset, 0);
 }

 void handleFileDownload(SOCKET clientSocket, const char* buffer)
 {
     char bufferSend[BUFFER_SIZE] = {};
     int offset = 0;

     in_addr clientIPAddr;
     memcpy(&clientIPAddr, buffer, sizeof(clientIPAddr));
     char clientIP[INET_ADDRSTRLEN];
     inet_ntop(AF_INET, &clientIPAddr, clientIP, INET_ADDRSTRLEN);
     offset += sizeof(clientIPAddr);

     uint16_t clientPortNum = ntohs(*(uint16_t*)(buffer + offset));
     offset += sizeof(clientPortNum);

     uint32_t filenameLen = ntohl(*(uint32_t*)(buffer + offset));
     offset += sizeof(filenameLen);
     std::string filename(buffer + offset, filenameLen);

     std::string filePath = path + "/" + filename;
     if (!std::filesystem::exists(filePath))
     {
         std::cerr << "File not found: " << filePath << std::endl;
         char errorMsg = static_cast<char>(CMDID::DOWNLOAD_ERROR);
         send(clientSocket, &errorMsg, 1, 0);
         return;
     }

     bufferSend[0] = static_cast<char>(CMDID::RSP_DOWNLOAD);
     offset = 1;

     uint32_t fileSize = static_cast<uint32_t>(std::filesystem::file_size(filePath));
     in_addr serverIP;
     inet_pton(AF_INET, serverIPAddress, &serverIP);
     memcpy(bufferSend + offset, &serverIP, sizeof(serverIP));
     offset += sizeof(serverIP);

     uint16_t udpPortNum = htons(static_cast<uint16_t>(std::stoi(portNumber_UDP)));
     memcpy(bufferSend + offset, &udpPortNum, sizeof(udpPortNum));
     offset += sizeof(udpPortNum);

     uint32_t sessionID = sessionCounter++;
     uint32_t sessionIDNetwork = htonl(sessionID);
     memcpy(bufferSend + offset, &sessionIDNetwork, sizeof(sessionIDNetwork));
     offset += sizeof(sessionIDNetwork);

     uint32_t fileSizeNet = htonl(fileSize);
     memcpy(bufferSend + offset, &fileSizeNet, sizeof(fileSizeNet));
     offset += sizeof(fileSizeNet);

     uint32_t filenameLenNet = htonl(filenameLen);
     memcpy(bufferSend + offset, &filenameLenNet, sizeof(filenameLenNet));
     offset += sizeof(filenameLenNet);

     memcpy(bufferSend + offset, filename.c_str(), filename.size());
     offset += (int)filename.size();

     send(clientSocket, bufferSend, offset, 0);

     std::thread fileThread(sendFileToClient, udpSocket, sessionID, filename,
         clientPortNum, clientIP, sessionIDNetwork,
         fileSizeNet, filePath);
     fileThread.detach();
 }

void sendFileToClient(int udpSock, uint32_t sessionIdentifier,
    std::string fileTitle, uint16_t clientPort, std::string clientIPAddress, uint32_t sessionIDNet,
    uint32_t fileSizeNetwork, std::string fileLocation)
{
    std::ifstream fileStream(fileLocation, std::ios::binary);
    if (!fileStream) {
        std::cerr << "Error: Unable to open file!" << std::endl;
        return;
    }
    char dataPacketBuffer[2000] = {};
    size_t chunkSize = 0;
    uint32_t filePosition = 0;

    if (sessionSeqNum.find(sessionIdentifier) == sessionSeqNum.end()) {
        sessionSeqNum[sessionIdentifier] = 1;
    }

    uint32_t packetNumber = sessionSeqNum[sessionIdentifier];

    sockaddr_in clientAddress;
    clientAddress.sin_family = AF_INET;
    clientAddress.sin_port = htons(clientPort);
    inet_pton(AF_INET, clientIPAddress.c_str(), &clientAddress.sin_addr);

    while (true)
    {
        int bufferOffset = 0;

        fileStream.read(dataPacketBuffer + 20, 1024);
        chunkSize = fileStream.gcount();

        if (chunkSize == 0) {
            break;
        }

        uint32_t sessionNet = htonl(sessionIdentifier);
        memcpy(dataPacketBuffer + bufferOffset, &sessionNet, sizeof(sessionNet));
        bufferOffset += sizeof(sessionNet);

        memcpy(dataPacketBuffer + bufferOffset, &fileSizeNetwork, sizeof(fileSizeNetwork));
        bufferOffset += sizeof(fileSizeNetwork);

        uint32_t filePositionNet = htonl(filePosition);
        memcpy(dataPacketBuffer + bufferOffset, &filePositionNet, sizeof(filePositionNet));
        bufferOffset += sizeof(filePositionNet);

        uint32_t fileChunkLength = htonl((uint32_t)chunkSize);
        memcpy(dataPacketBuffer + bufferOffset, &fileChunkLength, sizeof(fileChunkLength));
        bufferOffset += sizeof(fileChunkLength);

        uint32_t sequenceNumNet = htonl(packetNumber);
        memcpy(dataPacketBuffer + bufferOffset, &sequenceNumNet, sizeof(sequenceNumNet));
        bufferOffset += sizeof(sequenceNumNet);

        bufferOffset += (int)chunkSize;

        bool acknowledgementReceived = false;
        uint32_t acknowledgementNum;

        while (!acknowledgementReceived)
        {
            int bytesSent = sendto(udpSock, dataPacketBuffer, bufferOffset, 0, (sockaddr*)&clientAddress,
                sizeof(clientAddress));
            std::cout << "Bytes Sent: " << bytesSent << '\n';

            fd_set readFileDescriptors;
            FD_ZERO(&readFileDescriptors);
            FD_SET(udpSock, &readFileDescriptors);

            timeval timeLimit;
            timeLimit.tv_sec = 2;
            timeLimit.tv_usec = 0;

            sockaddr_in receivedClientAddr;
            int clientAddrSize = sizeof(receivedClientAddr);

            int selectResult = select(static_cast<int>(udpSock) + 1, &readFileDescriptors, nullptr, nullptr, &timeLimit);

            if (selectResult > 0) {
                char acknowledgementBuffer[4];
                int receivedBytes = recvfrom(
                    udpSock, acknowledgementBuffer, sizeof(acknowledgementBuffer), 0,
                    (sockaddr*)&receivedClientAddr, &clientAddrSize
                );

                acknowledgementNum = ntohl(*(uint32_t*)(acknowledgementBuffer));

                if (acknowledgementNum == packetNumber)
                {
                    acknowledgementReceived = true;
                    sessionSeqNum[sessionIdentifier] = ++packetNumber;
                }
                else
                {
                    std::cout << "[UDP] Incorrect ACK received (expected " << packetNumber << ", got " << acknowledgementNum << "). Resending...\n";
                }
            }
            else
            {
                std::cout << "Select result: " << selectResult << '\n';
                std::cout << "[UDP] ACK Timeout! Resending packet with packet number: " << packetNumber << "\n";
            }
        }

        filePosition += (uint32_t)chunkSize;
    }

    fileStream.close();
    std::cout << fileTitle << " sent\n";
}
