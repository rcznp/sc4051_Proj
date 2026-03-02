#include <iostream>
#include <cstring>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <chrono>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
#endif

#include "../common/protocol.h"
#include "../common/marshaller.h"

#define BUFFER_SIZE 1024
#define PORT 2222
bool readUint32(uint32_t &value) {
    std::cin >> value;
    if (std::cin.fail()) {
        std::cin.clear();
        std::cin.ignore(10000, '\n');
        return false;
    }
    std::cin.ignore(10000, '\n');
    return true;
}

bool readFloatSafe(float &value) {
    std::cin >> value;
    if (std::cin.fail()) {
        std::cin.clear();
        std::cin.ignore(10000, '\n');
        return false;
    }
    std::cin.ignore(10000, '\n');
    return true;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
    std::cerr << "Usage: ./client <server_ip> <port>\n";
    return 1;
    }

    const char* serverIP = argv[1];
    int port = std::stoi(argv[2]);

    srand(time(nullptr));
    uint32_t clientId = rand();   // Unique per client instance
    uint32_t requestId = 1;

    std::cout << "Client started with clientId: " << clientId << "\n";

    #ifdef _WIN32
    WSADATA wsa;
        if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
            std::cerr << "WSAStartup failed\n";
            return 1;
        }
        SOCKET sockfd;
    #else
        int sockfd;
    #endif
    struct sockaddr_in serverAddr;
    char buffer[BUFFER_SIZE];

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::cerr << "Socket creation failed\n";
        return 1;
    }

    #ifdef _WIN32
    DWORD timeout = 5000;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,
               (const char*)&timeout, sizeof(timeout));
    #else
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,
                &tv, sizeof(tv));
    #endif

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    if (inet_pton(AF_INET, serverIP, &serverAddr.sin_addr) <= 0) {
        std::cerr << "Invalid server IP address\n";
        return 1;
    }

    while (true) {

        std::cout << "\n===== Banking Menu =====\n";
        std::cout << "1. Open Account\n";
        std::cout << "2. Deposit\n";
        std::cout << "3. Withdraw\n";
        std::cout << "4. Close Account\n";
        std::cout << "5. Monitor Accounts\n";
        std::cout << "6. Exit\n";
        std::cout << "Select option: ";

        uint32_t choice;
        uint32_t monitorSeconds = 0;

        if (!readUint32(choice)) {
            std::cout << "Invalid menu option.\n";
            continue;
        }
        if (choice == 6) break;

        std::vector<uint8_t> request;

        // --- HEADER ---
        appendInt(request, clientId);
        appendInt(request, requestId++);
        uint32_t opCode;
        switch (choice) {
            case 1: opCode = OP_OPEN_ACCOUNT; break;
            case 2: opCode = OP_DEPOSIT; break;
            case 3: opCode = OP_WITHDRAW; break;
            case 4: opCode = OP_CLOSE_ACCOUNT; break;
            case 5: opCode = OP_REGISTER_MONITOR; break;
            default:
                std::cout << "Invalid option\n";
                continue;
        }

        appendInt(request, opCode);

        std::vector<uint8_t> payload;

        if (opCode == OP_OPEN_ACCOUNT) {

            std::string name, password;
            float balance;
            uint32_t currency;

            std::cout << "Enter Name: ";
            std::getline(std::cin, name);

            std::cout << "Enter Password: ";
            std::getline(std::cin, password);

            std::cout << "Currency (1=USD,2=SGD,3=EUR): ";
            std::cin >> currency;

            std::cout << "Initial Balance: ";
            std::cin >> balance;
            std::cin.ignore();

            appendString(payload, name);
            appendString(payload, password);
            appendInt(payload, currency);
            appendFloat(payload, balance);
        }
        else if (opCode == OP_DEPOSIT || opCode == OP_WITHDRAW) {

            uint32_t accNo;
            std::string password;
            float amount;

            std::cout << "Account Number: ";
            if (!readUint32(accNo)) continue;

            std::cout << "Password: ";
            std::getline(std::cin, password);

            std::cout << "Amount: ";
            if (!readFloatSafe(amount)) continue;

            appendInt(payload, accNo);
            appendString(payload, password);
            appendFloat(payload, amount);
        }
        else if (opCode == OP_CLOSE_ACCOUNT) {

            uint32_t accNo;
            std::string password;

            std::cout << "Account Number: ";
            if (!readUint32(accNo)) continue;

            std::cout << "Password: ";
            std::getline(std::cin, password);

            appendInt(payload, accNo);
            appendString(payload, password);
        }
        else if (opCode == OP_REGISTER_MONITOR) {

            std::cout << "Monitor interval (seconds): ";
            if (!readUint32(monitorSeconds)) continue;

            appendInt(payload, monitorSeconds);
        }
        else {
            std::cout << "Invalid option\n";
            continue;
        }

        appendInt(request, payload.size());
        request.insert(request.end(), payload.begin(), payload.end());

        sendto(sockfd,
            reinterpret_cast<const char*>(request.data()),
            static_cast<int>(request.size()),
            0,
            (struct sockaddr*)&serverAddr,
            sizeof(serverAddr));

        int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, NULL, NULL);

        if (n < 0) {
            std::cout << "No response (timeout)\n";
            continue;
        }

        std::vector<uint8_t> response(buffer, buffer + n);
        size_t offset = 0;

        uint32_t respClientId = readInt(response, offset);
        uint32_t respReqId    = readInt(response, offset);
        uint32_t status       = readInt(response, offset);
        uint32_t payloadLen   = readInt(response, offset);

        if (respClientId != clientId) {
            std::cout << "Received packet for different client\n";
            continue;
        }

        if (status == STATUS_SUCCESS) {

            if (opCode == OP_OPEN_ACCOUNT) {
                uint32_t accNo = readInt(response, offset);
                std::cout << "Account Created! Number: " << accNo << "\n";
            }
            else if (opCode == OP_DEPOSIT || opCode == OP_WITHDRAW) {
                float newBalance = readFloat(response, offset);
                std::cout << "New Balance: " << newBalance << "\n";
            }
            else if (opCode == OP_CLOSE_ACCOUNT) {
                std::cout << "Account successfully closed.\n";
            }
            else if (opCode == OP_REGISTER_MONITOR) {

                std::cout << "Monitoring started...\n";
                auto start = std::chrono::steady_clock::now();

                while (true) {

                    int n2 = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, NULL, NULL);

                    if (n2 > 0) {

                        std::vector<uint8_t> msg(buffer, buffer + n2);
                        size_t offset2 = 0;

                        uint32_t cbClientId = readInt(msg, offset2);
                        uint32_t cbReqId    = readInt(msg, offset2);
                        uint32_t cbStatus   = readInt(msg, offset2);
                        uint32_t cbLen      = readInt(msg, offset2);

                        uint32_t operation = readInt(msg, offset2);
                        uint32_t accNo     = readInt(msg, offset2);
                        float balance      = readFloat(msg, offset2);

                        std::cout << "[UPDATE] "
                                  << "Account " << accNo
                                  << " | Balance: " << balance << "\n";
                    }

                    if (std::chrono::steady_clock::now() - start >
                        std::chrono::seconds(monitorSeconds)) {
                        break;
                    }
                }

                std::cout << "Monitoring ended.\n";
            }
        }
        else {
            std::string errorMsg = readString(response, offset);
            std::cout << "Error: " << errorMsg << "\n";
        }
    }

    #ifdef _WIN32
        closesocket(sockfd);
        WSACleanup();
    #else
        close(sockfd);
    #endif
    return 0;
}