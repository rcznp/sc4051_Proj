#include <iostream>
#include <cstring>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <iomanip>

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
#define MAX_RETRIES 4

static const char *CLR_RED = "\033[31m";
static const char *CLR_GREEN = "\033[32m";
static const char *CLR_RESET = "\033[0m";

bool readUint32(uint32_t &value)
{
    std::cin >> value;
    if (std::cin.fail())
    {
        std::cin.clear();
        std::cin.ignore(10000, '\n');
        return false;
    }
    std::cin.ignore(10000, '\n');
    return true;
}

bool readFloatSafe(float &value)
{
    std::cin >> value;
    if (std::cin.fail())
    {
        std::cin.clear();
        std::cin.ignore(10000, '\n');
        return false;
    }
    std::cin.ignore(10000, '\n');
    return true;
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        std::cerr << "Usage: ./client <server_ip> <port> [loss_probability]\n";
        return 1;
    }

    const char *serverIP = argv[1];
    int port = std::stoi(argv[2]);
    float lossProbability = 0.0f;
    if (argc >= 4)
    {
        lossProbability = std::stof(argv[3]) / 100.0f;
    }

    srand(time(nullptr));
    uint32_t clientId = rand(); // Unique per client instance
    uint32_t requestId = 1;

    std::cout << "Client started with clientId: " << clientId
              << " | Loss probability: " << lossProbability * 100 << "%"
              << "\n";

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
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
    if (sockfd < 0)
    {
        std::cerr << "Socket creation failed\n";
        return 1;
    }

#ifdef _WIN32
    DWORD timeout = 5000;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,
               (const char *)&timeout, sizeof(timeout));
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
    if (inet_pton(AF_INET, serverIP, &serverAddr.sin_addr) <= 0)
    {
        std::cerr << "Invalid server IP address\n";
        return 1;
    }

    while (true)
    {

        std::cout << "\n===== Banking Menu =====\n";
        std::cout << "1. Open Account\n";
        std::cout << "2. Deposit\n";
        std::cout << "3. Withdraw\n";
        std::cout << "4. Close Account\n";
        std::cout << "5. Monitor Accounts\n";
        std::cout << "6. Check Transaction History\n";
        std::cout << "7. Transfer Money\n";
        std::cout << "8. Test Invocation Semantics\n";
        std::cout << "9. Exit\n";
        std::cout << "Select option: ";

        uint32_t choice;
        uint32_t monitorSeconds = 0;

        if (!readUint32(choice))
        {
            std::cout << "Invalid menu option.\n";
            continue;
        }
        if (choice == 9)
            break;

        std::vector<uint8_t> request;

        // --- HEADER ---
        uint32_t currentRequestId = requestId++;
        appendInt(request, clientId);
        appendInt(request, currentRequestId);
        // === Option 8: Test Invocation Semantics ===
        if (choice == 8)
        {
            std::cout << "\n=== Invocation Semantics Test ===\n";
            std::cout << "This sends the SAME deposit request multiple times to demonstrate\n";
            std::cout << "the difference between at-least-once and at-most-once.\n";
            std::cout << "The server's mode determines the behavior.\n\n";

            std::string name;
            uint32_t accNo;
            std::string password;
            uint32_t currency;
            float amount;
            uint32_t numSends;

            std::cout << "Name: ";
            std::getline(std::cin, name);
            std::cout << "Account Number: ";
            if (!readUint32(accNo))
            {
                std::cout << "Invalid input.\n";
                continue;
            }
            std::cout << "Password: ";
            std::getline(std::cin, password);
            std::cout << "Currency (1=USD,2=SGD,3=EUR): ";
            if (!readUint32(currency))
            {
                std::cout << "Invalid input.\n";
                continue;
            }
            std::cout << "Deposit Amount: ";
            if (!readFloatSafe(amount))
            {
                std::cout << "Invalid input.\n";
                continue;
            }
            std::cout << "Number of times to send the SAME request: ";
            if (!readUint32(numSends))
            {
                std::cout << "Invalid input.\n";
                continue;
            }

            // Build ONE request with a single requestId
            uint32_t testRequestId = requestId++;
            std::vector<uint8_t> testRequest;
            appendInt(testRequest, clientId);
            appendInt(testRequest, testRequestId);
            appendInt(testRequest, OP_DEPOSIT);

            std::vector<uint8_t> testPayload;
            appendString(testPayload, name);
            appendInt(testPayload, accNo);
            appendString(testPayload, password);
            appendInt(testPayload, currency);
            appendFloat(testPayload, amount);

            appendInt(testRequest, testPayload.size());
            testRequest.insert(testRequest.end(), testPayload.begin(), testPayload.end());

            // Send the SAME request multiple times
            bool testFailed = false;
            for (uint32_t i = 0; i < numSends; i++)
            {
                std::cout << "\n[TEST SEND " << (i + 1) << "/" << numSends
                          << "] requestId=" << testRequestId << "\n";

                sendto(sockfd,
                       reinterpret_cast<const char *>(testRequest.data()),
                       static_cast<int>(testRequest.size()),
                       0,
                       (struct sockaddr *)&serverAddr,
                       sizeof(serverAddr));

                int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, NULL, NULL);
                if (n > 0)
                {
                    std::vector<uint8_t> resp(buffer, buffer + n);
                    size_t off = 0;
                    readInt(resp, off); // clientId
                    readInt(resp, off); // requestId
                    uint32_t st = readInt(resp, off);
                    readInt(resp, off); // payloadLen

                    if (st == STATUS_SUCCESS)
                    {
                        float bal = readFloat(resp, off);
                        std::cout << "  Response: SUCCESS | Balance: " << bal << "\n";
                    }
                    else
                    {
                        std::string err = readString(resp, off);
                        std::cout << "  Response: ERROR | " << err << "\n";
                        std::cout << "\nTest aborted due to error. Please check your inputs.\n";
                        testFailed = true;
                        break;
                    }
                }
                else
                {
                    std::cout << "  No response (timeout)\n";
                    std::cout << "\nTest aborted due to timeout.\n";
                    testFailed = true;
                    break;
                }
            }

            if (!testFailed)
            {
                std::cout << "\n=== Test Complete ===\n";
                std::cout << "If server is AT-LEAST-ONCE: balance changed " << numSends << " times (duplicates re-executed).\n";
                std::cout << "If server is AT-MOST-ONCE: balance changed only ONCE (duplicates returned cached reply).\n";
            }
            continue;
        }

        uint32_t opCode;
        switch (choice)
        {
        case 1:
            opCode = OP_OPEN_ACCOUNT;
            break;
        case 2:
            opCode = OP_DEPOSIT;
            break;
        case 3:
            opCode = OP_WITHDRAW;
            break;
        case 4:
            opCode = OP_CLOSE_ACCOUNT;
            break;
        case 5:
            opCode = OP_REGISTER_MONITOR;
            break;
        case 6:
            opCode = OP_CHECK_HISTORY;
            break;
        case 7:
            opCode = OP_TRANSFER;
            break;
        default:
            std::cout << "Invalid option\n";
            continue;
        }

        appendInt(request, opCode);

        std::vector<uint8_t> payload;

        if (opCode == OP_OPEN_ACCOUNT)
        {

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
        else if (opCode == OP_DEPOSIT || opCode == OP_WITHDRAW)
        {

            std::string name;
            uint32_t accNo;
            std::string password;
            uint32_t currency;
            float amount;

            std::cout << "Name: ";
            std::getline(std::cin, name);

            std::cout << "Account Number: ";
            if (!readUint32(accNo))
            {
                std::cout << "Invalid input. Account number must be a number.\n";
                continue;
            }

            std::cout << "Password: ";
            std::getline(std::cin, password);

            std::cout << "Currency (1=USD,2=SGD,3=EUR): ";
            if (!readUint32(currency))
            {
                std::cout << "Invalid input. Currency must be 1, 2, or 3.\n";
                continue;
            }

            std::cout << "Amount: ";
            if (!readFloatSafe(amount))
            {
                std::cout << "Invalid input. Amount must be a number.\n";
                continue;
            }

            appendString(payload, name);
            appendInt(payload, accNo);
            appendString(payload, password);
            appendInt(payload, currency);
            appendFloat(payload, amount);
        }
        else if (opCode == OP_CLOSE_ACCOUNT)
        {

            std::string name;
            uint32_t accNo;
            std::string password;

            std::cout << "Name: ";
            std::getline(std::cin, name);

            std::cout << "Account Number: ";
            if (!readUint32(accNo))
            {
                std::cout << "Invalid input. Account number must be a number.\n";
                continue;
            }

            std::cout << "Password: ";
            std::getline(std::cin, password);

            appendString(payload, name);
            appendInt(payload, accNo);
            appendString(payload, password);
        }
        else if (opCode == OP_REGISTER_MONITOR)
        {

            std::cout << "Monitor interval (seconds): ";
            if (!readUint32(monitorSeconds))
            {
                std::cout << "Invalid input. Interval must be a number.\n";
                continue;
            }

            appendInt(payload, monitorSeconds);
        }
        else if (opCode == OP_CHECK_HISTORY)
        {

            std::string name;
            uint32_t accNo;
            std::string password;

            std::cout << "Name: ";
            std::getline(std::cin, name);

            std::cout << "Account Number: ";
            if (!readUint32(accNo))
            {
                std::cout << "Invalid input. Account number must be a number.\n";
                continue;
            }

            std::cout << "Password: ";
            std::getline(std::cin, password);

            appendString(payload, name);
            appendInt(payload, accNo);
            appendString(payload, password);
        }
        else if (opCode == OP_TRANSFER)
        {

            std::string name;
            uint32_t srcAccNo;
            std::string password;
            uint32_t currency;
            float amount;
            uint32_t destAccNo;

            std::cout << "Your Name: ";
            std::getline(std::cin, name);

            std::cout << "Your Account Number: ";
            if (!readUint32(srcAccNo))
            {
                std::cout << "Invalid input. Account number must be a number.\n";
                continue;
            }

            std::cout << "Password: ";
            std::getline(std::cin, password);

            std::cout << "Currency (1=USD,2=SGD,3=EUR): ";
            if (!readUint32(currency))
            {
                std::cout << "Invalid input. Currency must be 1, 2, or 3.\n";
                continue;
            }

            std::cout << "Amount to Transfer: ";
            if (!readFloatSafe(amount))
            {
                std::cout << "Invalid input. Amount must be a number.\n";
                continue;
            }

            std::cout << "Destination Account Number: ";
            if (!readUint32(destAccNo))
            {
                std::cout << "Invalid input. Account number must be a number.\n";
                continue;
            }

            appendString(payload, name);
            appendInt(payload, srcAccNo);
            appendString(payload, password);
            appendInt(payload, currency);
            appendFloat(payload, amount);
            appendInt(payload, destAccNo);
        }
        else
        {
            std::cout << "Invalid option\n";
            continue;
        }

        appendInt(request, payload.size());
        request.insert(request.end(), payload.begin(), payload.end());

        // === Send with retry logic ===
        bool gotResponse = false;
        int retries = 0;

        while (!gotResponse && retries <= MAX_RETRIES)
        {
            if (retries > 0)
            {
                std::cout << "[RETRY " << retries << "/" << MAX_RETRIES
                          << "] Resending request (requestId: " << currentRequestId << ")\n";
            }

            // Simulate request loss on client side
            bool dropSend = false;
            if (lossProbability > 0.0f)
            {
                float roll = static_cast<float>(rand()) / RAND_MAX;
                std::cout << "[SIMULATED LOSS] roll = " << roll << std::endl;
                if (roll < lossProbability)
                {
                    std::cout << "[SIMULATED LOSS] Dropping outgoing request\n";
                    dropSend = true;
                }
            }

            if (!dropSend)
            {
                int sent = sendto(sockfd,
                                  reinterpret_cast<const char *>(request.data()),
                                  static_cast<int>(request.size()),
                                  0,
                                  (struct sockaddr *)&serverAddr,
                                  sizeof(serverAddr));

                std::cout << "[DEBUG] Sending to "
                          << serverIP << ":" << port
                          << " | bytes: " << sent
                          << std::endl;

#ifdef _WIN32
                if (sent == SOCKET_ERROR)
                {
                    std::cout << "WSA Error: " << WSAGetLastError() << std::endl;
                }
#endif
            }

            int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, NULL, NULL);

            if (n < 0)
            {
                retries++;
                if (retries > MAX_RETRIES)
                {
                    std::cout << "No response after " << MAX_RETRIES << " retries. Giving up.\n";
                }
                else
                {
                    std::cout << "No response (timeout).\n";
                }
                continue;
            }

            std::vector<uint8_t> response(buffer, buffer + n);
            size_t offset = 0;

            uint32_t respClientId = readInt(response, offset);
            uint32_t respReqId = readInt(response, offset);
            uint32_t status = readInt(response, offset);
            uint32_t payloadLen = readInt(response, offset);

            if (respClientId != clientId)
            {
                std::cout << "Received packet for different client\n";
                continue;
            }

            gotResponse = true;

            if (status == STATUS_SUCCESS)
            {

                if (opCode == OP_OPEN_ACCOUNT)
                {
                    uint32_t accNo = readInt(response, offset);
                    std::cout << "Account Created! Number: " << accNo << "\n";
                }
                else if (opCode == OP_DEPOSIT || opCode == OP_WITHDRAW)
                {
                    float newBalance = readFloat(response, offset);
                    std::cout << "New Balance: " << newBalance << "\n";
                }
                else if (opCode == OP_CLOSE_ACCOUNT)
                {
                    std::cout << "Account successfully closed.\n";
                }
                else if (opCode == OP_CHECK_HISTORY)
                {
                    uint32_t numTransactions = readInt(response, offset);
                    std::cout << "\n--- Transaction History (" << numTransactions << " entries) ---\n";
                    float currentBalance = 0.0f;
                    for (uint32_t i = 0; i < numTransactions; i++)
                    {
                        uint32_t op = readInt(response, offset);
                        float amount = readFloat(response, offset);
                        float balAfter = readFloat(response, offset);
                        std::string desc = readString(response, offset);
                        currentBalance = balAfter;

                        std::string opStr;
                        const char *color = "";
                        const char *reset = "";
                        std::string amountLabel = "Amount";
                        switch (op)
                        {
                        case OP_DEPOSIT:
                            opStr = "DEPOSIT";
                            color = CLR_GREEN;
                            reset = CLR_RESET;
                            amountLabel = "Added";
                            break;
                        case OP_WITHDRAW:
                            opStr = "WITHDRAW";
                            color = CLR_RED;
                            reset = CLR_RESET;
                            amountLabel = "Took";
                            break;
                        case OP_TRANSFER:
                            if (desc.rfind("Transfer in", 0) == 0)
                            {
                                opStr = "TRANSFER IN";
                                color = CLR_GREEN;
                                reset = CLR_RESET;
                                amountLabel = "Received";
                            }
                            else
                            {
                                opStr = "TRANSFER OUT";
                                color = CLR_RED;
                                reset = CLR_RESET;
                                amountLabel = "Sent";
                            }
                            break;
                        default:
                            opStr = "OTHER";
                            break;
                        }

                        std::cout << "  " << (i + 1) << ". " << color << opStr << reset
                                  << " | " << amountLabel << ": $"
                                  << std::fixed << std::setprecision(2) << amount
                                  << " | Balance After: $"
                                  << std::fixed << std::setprecision(2) << balAfter
                                  << " | " << desc << "\n";
                    }

                    if (numTransactions > 0)
                    {
                        std::cout << "Current Balance: $"
                                  << std::fixed << std::setprecision(2)
                                  << currentBalance << "\n";
                    }
                    else
                    {
                        std::cout << "Current Balance: $0.00 (no transactions yet)\n";
                    }

                    std::cout << "--- End of History ---\n";
                }
                else if (opCode == OP_TRANSFER)
                {
                    float srcBalance = readFloat(response, offset);
                    readFloat(response, offset); // skip destination balance
                    std::cout << "Transfer successful!\n";
                    std::cout << "  Your new balance: " << srcBalance << "\n";
                }
                else if (opCode == OP_REGISTER_MONITOR)
                {

                    std::cout << "Monitoring started...\n";
                    auto start = std::chrono::steady_clock::now();

                    while (true)
                    {

                        int n2 = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, NULL, NULL);

                        if (n2 > 0)
                        {

                            std::vector<uint8_t> msg(buffer, buffer + n2);
                            size_t offset2 = 0;

                            uint32_t cbClientId = readInt(msg, offset2);
                            uint32_t cbReqId = readInt(msg, offset2);
                            uint32_t cbStatus = readInt(msg, offset2);
                            uint32_t cbLen = readInt(msg, offset2);

                            std::string eventLog = readString(msg, offset2);
                            std::cout << eventLog << "\n";
                        }

                        if (std::chrono::steady_clock::now() - start >
                            std::chrono::seconds(monitorSeconds))
                        {
                            break;
                        }
                    }

                    std::cout << "Monitoring ended.\n";
                }
            }
            else
            {
                std::string errorMsg = readString(response, offset);
                std::cout << "Error: " << errorMsg << "\n";
            }
        } // end retry loop
    }

#ifdef _WIN32
    closesocket(sockfd);
    WSACleanup();
#else
    close(sockfd);
#endif
    return 0;
}
