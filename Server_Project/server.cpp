/* Start Header
*****************************************************************/
/*!
\file server.cpp
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
 SOCKET udpSocket;

#define MAX_STR_LEN         1000
#define chunkSize            1024
#define TIMEOUT_MS 2000
 
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
 
void sendFileToClient(int udpSocket, uint32_t sessionID,
    std::string filename, uint16_t clientPortNum, std::string clientIP, uint32_t sessionIDNetwork,
    uint32_t fileSizeNet, std::string filePath);

 std::map<SOCKET, std::pair<std::string, uint16_t>> connectedClients;
 //void recvUdpMsgs(int clientUdpSocket);
 std::string path;
 char serverIPAddr[MAX_STR_LEN];
 std::string UDPportNumber;
 static uint32_t sessionCounter = 1;
 sockaddr_in udpAddr;
 std::vector<std::pair<std::string, uint32_t>> fileList;
 std::unordered_map<uint32_t, uint32_t> sessionSeqNum;
 
 int main()
 {
     //constexpr uint16_t port = 2048;
 
     //const std::string portString = std::to_string(port);
 
     // Get Port Number
     std::string TCPportNumber;
     std::cout << "Server TCP Port Number: ";
     std::getline(std::cin, TCPportNumber);
     std::cout << "Server UDP Port Number: ";
     std::getline(std::cin, UDPportNumber);
     std::cout << "Path: ";
     std::getline(std::cin, path);
 
     std::string TCPportString = TCPportNumber;
     uint16_t udpPort = static_cast<uint16_t>(std::stoi(UDPportNumber));
 
     
 
     // -------------------------------------------------------------------------
     // Start up Winsock, asking for version 2.2.
     //
     // WSAStartup()
     // -------------------------------------------------------------------------
 
     // This object holds the information about the version of Winsock that we
     // are using, which is not necessarily the version that we requested.
     WSADATA wsaData{};
 
     // Initialize Winsock. You must call WSACleanup when you are finished.
     // As this function uses a reference counter, for each call to WSAStartup,
     // you must call WSACleanup or suffer memory issues.
     int errorCode = WSAStartup(MAKEWORD(2, 2), &wsaData);
     if (errorCode != NO_ERROR)
     {
         std::cerr << "WSAStartup() failed." << std::endl;
         return errorCode;
     }
 
     //std::cout
     //	<< "Winsock version: "
     //	<< static_cast<int>(LOBYTE(wsaData.wVersion))
     //	<< "."
     //	<< static_cast<int>(HIBYTE(wsaData.wVersion))
     //	<< "\n"
     //	<< std::endl;
 
 
     // -------------------------------------------------------------------------
     // Resolve own host name into IP addresses (in a singly-linked list).
     //
     // getaddrinfo()
     // -------------------------------------------------------------------------
 
     // Object hints indicates which protocols to use to fill in the info.
     addrinfo hints{};
     SecureZeroMemory(&hints, sizeof(hints));
     hints.ai_family = AF_INET;			// IPv4
     // For UDP use SOCK_DGRAM instead of SOCK_STREAM.
     hints.ai_socktype = SOCK_STREAM;	// Reliable delivery
     // Could be 0 for autodetect, but reliable delivery over IPv4 is always TCP.
     hints.ai_protocol = IPPROTO_TCP;	// TCP
     // Create a passive socket that is suitable for bind() and listen().
     hints.ai_flags = AI_PASSIVE;
 
     char host[MAX_STR_LEN];
     gethostname(host, MAX_STR_LEN);
 
     addrinfo* info = nullptr;
     errorCode = getaddrinfo(host, TCPportString.c_str(), &hints, &info);
     if ((errorCode) || (info == nullptr))
     {
         std::cerr << "getaddrinfo() failed." << std::endl;
         WSACleanup();
         return errorCode;
     }
 
     /* PRINT SERVER IP ADDRESS / TCP PORT NUMBER / UDP PORT NUMBER */
     
     struct sockaddr_in* serverAddress = reinterpret_cast<struct sockaddr_in*> (info->ai_addr);
     inet_ntop(AF_INET, &(serverAddress->sin_addr), serverIPAddr, INET_ADDRSTRLEN);
     getnameinfo(info->ai_addr, static_cast <socklen_t> (info->ai_addrlen), serverIPAddr, sizeof(serverIPAddr), nullptr, 0, NI_NUMERICHOST);
     std::cout << std::endl;
     std::cout << "Server IP Address: " << serverIPAddr << std::endl;
     std::cout << "Server TCP Port Number: " << TCPportString << std::endl;
     std::cout << "Server UDP Port Number: " << udpPort << std::endl;
 
 
     // -------------------------------------------------------------------------
     // Create a socket and bind it to own network interface controller.
     //
     // socket()
     // bind()
     // -------------------------------------------------------------------------
 
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

     //UDP Protocol
     udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
     if (udpSocket == INVALID_SOCKET) {
         std::cerr << "Failed to create UDP socket." << std::endl;
         WSACleanup();
         return 1;
     }

     // Bind the UDP socket to the specified port
     udpAddr.sin_family = AF_INET;
     udpAddr.sin_addr.s_addr = INADDR_ANY; // Accept data from any address
     udpAddr.sin_port = htons(udpPort);

     if (bind(udpSocket, (sockaddr*)&udpAddr, sizeof(udpAddr)) == SOCKET_ERROR) {
         std::cerr << "Failed to bind UDP socket." << std::endl;
         closesocket(udpSocket);
         WSACleanup();
         return 1;
     }
 
 
     // -------------------------------------------------------------------------
     // Set a socket in a listening mode and accept 1 incoming client.
     //
     // listen()
     // accept()
     // -------------------------------------------------------------------------
 
     errorCode = listen(tcpSocket, SOMAXCONN);
     if (errorCode != NO_ERROR)
     {
         std::cerr << "TCPlisten() failed." << std::endl;
         closesocket(tcpSocket);
         WSACleanup();
         return 3;
     }

     //std::thread UdprecvThread(recvUdpMsgs, udpSocket);
     //UdprecvThread.detach();
 
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
             char clientIP[MAX_STR_LEN];
             inet_ntop(AF_INET, &clientAddrIn->sin_addr, clientIP, INET_ADDRSTRLEN);
             int clientPort = ntohs(clientAddrIn->sin_port);
 
             std::cout << '\n' << "Client IP Address: " << clientIP << '\n'
                 << "Client Port Number: " << clientPort << '\n';
 
             tq.produce(clientSocket);
             connectedClients[clientSocket] = { clientIP, clientPort };
         }
     }
 
     // -------------------------------------------------------------------------
     // Clean-up after Winsock.
     //
     // WSACleanup()
     // -------------------------------------------------------------------------
 
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


 //void recvUdpMsgs(int udpSocket)
 //{
 //    char udpRecvBuffer[1024];
 //    sockaddr_in clientAddr;
 //    int clientAddrSize = sizeof(clientAddr);


 //    while (true)
 //    {

 //        //UDP
 //        int udpBytesReceived = recvfrom(
 //            udpSocket, udpRecvBuffer, sizeof(udpRecvBuffer) - 1, 0,
 //            (sockaddr*)&clientAddr, &clientAddrSize
 //        );

 //        if (udpBytesReceived > 0) {
 //            udpRecvBuffer[udpBytesReceived] = '\0';

 //            char clientIP[INET_ADDRSTRLEN];
 //            inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
 //            uint16_t clientPort = ntohs(clientAddr.sin_port);

 //            std::cout << "Received UDP message from " << clientIP << ":" << clientPort
 //                << " - " << udpRecvBuffer << std::endl;

 //            std::string response = "Server Echo: " + std::string(udpRecvBuffer);

 //            sendto(udpSocket, response.c_str(), response.size(), 0,
 //                (sockaddr*)&clientAddr, sizeof(clientAddr));
 //            std::cout << "Sent UDP response to " << clientIP << ":" << clientPort << std::endl;
 //        }
 //    }

 //}

 void sendUdpMsgs(int udpSocket)
 {

 }

 std::vector<std::pair<std::string, uint32_t>> GetFiles(std::string path)
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
 
     // -------------------------------------------------------------------------
     // Receive some text and send it back.
     //
     // recv()
     // send()
     // -------------------------------------------------------------------------
 
     constexpr size_t BUFFER_SIZE = 1000;
     char buffer[BUFFER_SIZE];
     bool stay = true;

     //char udpRecvBuffer[1024];
     sockaddr_in clientAddr;
     int clientAddrSize = sizeof(clientAddr);

     //UDP
     //int udpBytesReceived = recvfrom(
     //    udpSocket, udpRecvBuffer, sizeof(udpRecvBuffer) - 1, 0,
     //    (sockaddr*)&clientAddr, &clientAddrSize
     //);

     while (true)
     {	
         //TCP
         const int bytesReceived = recv(
             clientSocket,
             buffer,
             BUFFER_SIZE - 1,
             0);
         if (bytesReceived == SOCKET_ERROR)
         {
             std::cerr << "recv() failed." << std::endl;
             break;
         }
         if (bytesReceived == 0)
         {
             std::cerr << "Graceful shutdown." << std::endl;
             break;
         }
         int cmdID = static_cast<int>(buffer[0]);
 
         buffer[bytesReceived] = '\0';
         if (cmdID == 4)
         {	
            char bufferSend[1000] = {};
            int offset = 0;

            bufferSend[offset] = static_cast<char>(RSP_LISTFILES);
            offset += 1;

            //get files into vector
            fileList = GetFiles(path);
             
            //get number of files
            size_t numFiles = fileList.size();
            uint16_t numFileNet = htons(static_cast<uint16_t>(numFiles));
            memcpy(bufferSend + offset, &numFileNet, sizeof(numFileNet));
            offset += sizeof(numFileNet);

            size_t lengthFileList = 4 * numFiles;
            for (const auto& file : fileList)
            {
                lengthFileList += static_cast<size_t>(file.second);
            }
            uint32_t lenFileListNet = htonl(static_cast<uint32_t>(lengthFileList));
            memcpy(bufferSend + offset, &lenFileListNet, sizeof(lenFileListNet));
            offset += sizeof(lenFileListNet);

            for (const auto& file : fileList)
            {
                uint32_t fileNameLenNet = htonl(static_cast<uint32_t>(file.second));
                memcpy(bufferSend + offset, &fileNameLenNet, sizeof(fileNameLenNet));
                offset += sizeof(fileNameLenNet);

                memcpy(bufferSend + offset, file.first.c_str(), file.first.size());
                offset += file.first.size();
            }
             
            send(clientSocket, bufferSend, offset, 0);
         }
         else if (cmdID == 2)
         {  
             //receive tcp /d directive
             char bufferSend[BUFFER_SIZE] = {};

             char clientIP[INET_ADDRSTRLEN];
             int offset = 1;

             //extract client IP address
             in_addr clientIPAddr;
             memcpy(&clientIPAddr, buffer + offset, sizeof(clientIPAddr));
             inet_ntop(AF_INET, &clientIPAddr, clientIP, INET_ADDRSTRLEN);
             offset += sizeof(clientIPAddr);

             //extract client Port Num address
             uint16_t clientPortNum = ntohs(*(uint16_t*)(buffer + offset));
             offset += 2;

             //extract filename length
             uint32_t filenameLen = ntohl(*(uint32_t*)(buffer + offset));
             offset += 4; //shift offset to text msg

             //extract filename
             std::string filename(buffer + offset, filenameLen);

             //FILEPATH IS WHERE THE USER INPUT AT THE START LOL
             std::string serverPath = path;
             std::string filePath = serverPath + "/" + filename;

             if (!std::filesystem::exists(filePath)) {
                 std::cerr << "File not found: " << filePath << std::endl;
                 char errorMsg = static_cast<char>(DOWNLOAD_ERROR);
                 send(clientSocket, &errorMsg, 1, 0);
             }
             else {
                 //send RSP_DOWNLOAD tcp msg to client
                 char ID = static_cast<char>(RSP_DOWNLOAD);
                 int offset = 0;
                 bufferSend[offset] = ID;
                 offset += 1;

                 uint32_t fileSize = static_cast<uint32_t>(std::filesystem::file_size(filePath));
                 //tcp send
                 //set server IP address
                 in_addr serverIP;
                 inet_pton(AF_INET, serverIPAddr, &serverIP);
                 memcpy(bufferSend + offset, &serverIP, sizeof(serverIP));
                 offset += sizeof(serverIP);

                 //set server port number
                 uint16_t udpPortNum = htons(static_cast<uint16_t>(std::stoi(UDPportNumber)));
                 memcpy(bufferSend + offset, &udpPortNum, sizeof(udpPortNum));
                 offset += sizeof(udpPortNum);

                 uint32_t sessionID = sessionCounter++;
                 uint32_t sessionIDNetwork = htonl(sessionID);
                 memcpy(bufferSend + offset, &sessionIDNetwork, sizeof(sessionIDNetwork));
                 offset += sizeof(sessionIDNetwork);

                 uint32_t fileSizeNet = htonl(fileSize);
                 memcpy(bufferSend + offset, &fileSizeNet, sizeof(fileSizeNet));
                 offset += sizeof(fileSizeNet);

                 uint32_t filenameLen = static_cast<uint32_t>(filename.size());
                 uint32_t filenameLenNet = htonl(filenameLen);
                 memcpy(bufferSend + offset, &filenameLenNet, sizeof(filenameLenNet));
                 offset += sizeof(filenameLenNet);

                 memcpy(bufferSend + offset, filename.c_str(), filename.size());
                 offset += filename.size();

                 send(clientSocket, bufferSend, offset, 0);

                 std::thread fileThread(sendFileToClient, udpSocket, sessionID, filename, 
                     clientPortNum, clientIP, sessionIDNetwork,
                     fileSizeNet, filePath);
                 fileThread.detach();
             }
         }
     }
 
 
     // -------------------------------------------------------------------------
     // Shut down and close sockets.
     //
     // shutdown()
     // closesocket()
     // -------------------------------------------------------------------------
 
     shutdown(clientSocket, SD_BOTH);
     closesocket(clientSocket);
     return stay;
 }
 

void sendFileToClient(int udpSocket, uint32_t sessionID,
    std::string filename, uint16_t clientPortNum, std::string clientIP, uint32_t sessionIDNetwork,
    uint32_t fileSizeNet, std::string filePath)
{
    //udp send
    // use sendto() to send file to client over 
    std::ifstream fileSent(filePath, std::ios::binary);
    if (!fileSent) {
        std::cerr << "Error: Unable to open file!" << std::endl;
        return;
    }
    char bufferDataPacket[2000] = {};
    size_t bytesRead = 0;
    uint32_t fileOffset = 0;

    if (sessionSeqNum.find(sessionID) == sessionSeqNum.end()) {
        sessionSeqNum[sessionID] = 1; // start at 1 for new session
    }

    uint32_t seqNum = sessionSeqNum[sessionID];

    sockaddr_in clientAddr;
    clientAddr.sin_family = AF_INET;
    clientAddr.sin_port = htons(clientPortNum);
    inet_pton(AF_INET, clientIP.c_str(), &clientAddr.sin_addr);

    //send chunks of file data to client
    while (true)
    {
        //send UDP datagram to client
        int offset = 0;

        // File Data (Copy actual file content into buffer)
        fileSent.read(bufferDataPacket + 20, 1024);
        bytesRead = fileSent.gcount();  // Get the actual number of bytes read

        if (bytesRead == 0) {
            break;  // Stop sending when no more data is available
        }

        //sessionID
        uint32_t sessionIDNet = htonl(sessionID);
        memcpy(bufferDataPacket + offset, &sessionIDNet, sizeof(sessionIDNet));
        offset += sizeof(sessionIDNet);

        //fileSize
        memcpy(bufferDataPacket + offset, &fileSizeNet, sizeof(fileSizeNet));
        offset += sizeof(fileSizeNet);

        //file offset
        uint32_t fileOffsetNet = htonl(fileOffset); // Convert to network byte order
        memcpy(bufferDataPacket + offset, &fileOffsetNet, sizeof(fileOffsetNet));
        offset += sizeof(fileOffsetNet);

        //File Data size
        uint32_t fileDataLength = htonl(bytesRead);
        memcpy(bufferDataPacket + offset, &fileDataLength, sizeof(fileDataLength));
        offset += sizeof(fileDataLength);

        //seqNUm
        uint32_t seqNumNet = htonl(seqNum);
        memcpy(bufferDataPacket + offset, &seqNumNet, sizeof(seqNumNet));
        offset += sizeof(seqNumNet);

        //file data alr in bufferDataPacket
        offset += bytesRead;

        bool ackReceived = false;
        uint32_t ackNum;

        //resend if theres an error
        while (!ackReceived)
        {
            int sent = sendto(udpSocket, bufferDataPacket, offset, 0, (sockaddr*)&clientAddr
                , sizeof(clientAddr));
            std::cout << "Bytes Sent: " << sent << '\n';

            // Set timeout for ACK reception
            fd_set readFds;
            FD_ZERO(&readFds);
            FD_SET(udpSocket, &readFds);

            timeval timeout;
            timeout.tv_sec = 2;  // 2 seconds timeout
            timeout.tv_usec = 0;

            sockaddr_in clientAddrRecv;
            int clientAddrSize = sizeof(clientAddrRecv);

            int result = select(static_cast<int>(udpSocket) + 1, &readFds, nullptr, nullptr, &timeout);

            if (result > 0) {
                // Receive ACK
                char ackBuffer[4];
                int udpBytesReceived = recvfrom(
                    udpSocket, ackBuffer, sizeof(ackBuffer), 0,
                    (sockaddr*)&clientAddrRecv, &clientAddrSize
                );

                ackNum = ntohl(*(uint32_t*)(ackBuffer));

                //dont resend, no error, go back to start of while(true)
                if (ackNum == seqNum)
                {
                    ackReceived = true;
                    sessionSeqNum[sessionID] = ++seqNum;
                }
                else
                {
                    std::cout << "[UDP] Incorrect ACK received (expected " << seqNum << ", got " << ackNum << "). Resending...\n";
                }
            }
            else
            {
                std::cout << "Result value: " << result << '\n';
                std::cout << "[UDP] ACK Timeout! Resending packet with SeqNum " << seqNum << "\n";
            }
        }

        fileOffset += bytesRead;

    }

    fileSent.close();
    std::cout << filename << " sent\n";
}