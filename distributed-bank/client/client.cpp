#include <iostream>
#include <cstring>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <iomanip>

using std::cin;
using std::cout;
using std::cerr;
using std::endl;
using std::getline;
using std::string;
using std::vector;
using std::fixed;
using std::setprecision;
using std::stof;
using std::stoi;
using namespace std::chrono;

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
    cin >> value;
    if (cin.fail())
    {
        cin.clear();
        cin.ignore(10000, '\n');
        return false;
    }
    cin.ignore(10000, '\n');
    return true;
}

bool readFloatSafe(float &value)
{
    cin >> value;
    if (cin.fail())
    {
        cin.clear();
        cin.ignore(10000, '\n');
        return false;
    }
    cin.ignore(10000, '\n');
    return true;
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        cerr << "Usage: ./client <server_ip> <port> [loss_probability]\n";
        return 1;
    }

    const char *serverIP = argv[1];
    int port = stoi(argv[2]);
    float lossProbability = 0.0f;
    if (argc >= 4)
    {
        lossProbability = stof(argv[3]) / 100.0f;
    }

    srand(time(nullptr));
    uint32_t clientId = rand(); // Unique per client instance
    uint32_t requestId = 1;

    cout << "Client started with clientId: " << clientId
         << " | Loss probability: " << lossProbability * 100 << "%"
         << "\n";

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        cerr << "WSAStartup failed\n";
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
        cerr << "Socket creation failed\n";
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
        cerr << "Invalid server IP address\n";
        return 1;
    }

    while (true)
    {

        cout << "\n===== Banking Menu =====\n";
        cout << "1. Open Account\n";
        cout << "2. Deposit\n";
        cout << "3. Withdraw\n";
        cout << "4. Close Account\n";
        cout << "5. Monitor Accounts\n";
        cout << "6. Check Transaction History\n";
        cout << "7. Transfer Money\n";
        cout << "8. Test Invocation Semantics\n";
        cout << "9. Exit\n";
        cout << "Select option: ";

        uint32_t choice;
        uint32_t monitorSeconds = 0;

        if (!readUint32(choice))
        {
            cout << "Invalid menu option.\n";
            continue;
        }
        if (choice == 9)
            break;

        vector<uint8_t> request;

        // --- HEADER ---
        uint32_t currentRequestId = requestId++;
        appendInt(request, clientId);
        appendInt(request, currentRequestId);
        // === Option 8: Test Invocation Semantics ===
        if (choice == 8)
        {
            cout << "\n=== Invocation Semantics Test ===\n";
            cout << "This sends the SAME deposit request multiple times to demonstrate\n";
            cout << "the difference between at-least-once and at-most-once.\n";
            cout << "The server's mode determines the behavior.\n\n";

            string name;
            uint32_t accNo;
            string password;
            uint32_t currency;
            float amount;
            uint32_t numSends;

            cout << "Name: ";
            getline(cin, name);
            cout << "Account Number: ";
            if (!readUint32(accNo))
            {
                cout << "Invalid input.\n";
                continue;
            }
            cout << "Password: ";
            getline(cin, password);
            cout << "Currency (1=USD,2=SGD,3=EUR): ";
            if (!readUint32(currency))
            {
                cout << "Invalid input.\n";
                continue;
            }
            cout << "Deposit Amount: ";
            if (!readFloatSafe(amount))
            {
                cout << "Invalid input.\n";
                continue;
            }
            cout << "Number of times to send the SAME request: ";
            if (!readUint32(numSends))
            {
                cout << "Invalid input.\n";
                continue;
            }

            // Build ONE request with a single requestId
            uint32_t testRequestId = requestId++;
            vector<uint8_t> testRequest;
            appendInt(testRequest, clientId);
            appendInt(testRequest, testRequestId);
            appendInt(testRequest, OP_DEPOSIT);

            vector<uint8_t> testPayload;
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
                cout << "\n[TEST SEND " << (i + 1) << "/" << numSends
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
                    vector<uint8_t> resp(buffer, buffer + n);
                    size_t off = 0;
                    readInt(resp, off); // clientId
                    readInt(resp, off); // requestId
                    uint32_t st = readInt(resp, off);
                    readInt(resp, off); // payloadLen

                    if (st == STATUS_SUCCESS)
                    {
                        float bal = readFloat(resp, off);
                        cout << "  Response: SUCCESS | Balance: " << bal << "\n";
                    }
                    else
                    {
                        string err = readString(resp, off);
                        cout << "  Response: ERROR | " << err << "\n";
                        cout << "\nTest aborted due to error. Please check your inputs.\n";
                        testFailed = true;
                        break;
                    }
                }
                else
                {
                    cout << "  No response (timeout)\n";
                    cout << "\nTest aborted due to timeout.\n";
                    testFailed = true;
                    break;
                }
            }

            if (!testFailed)
            {
                cout << "\n=== Test Complete ===\n";
                cout << "If server is AT-LEAST-ONCE: balance changed " << numSends << " times (duplicates re-executed).\n";
                cout << "If server is AT-MOST-ONCE: balance changed only ONCE (duplicates returned cached reply).\n";
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
            cout << "Invalid option\n";
            continue;
        }

        appendInt(request, opCode);

        vector<uint8_t> payload;

        if (opCode == OP_OPEN_ACCOUNT)
        {

            string name, password;
            float balance;
            uint32_t currency;

            cout << "Enter Name: ";
            getline(cin, name);

            cout << "Enter Password: ";
            getline(cin, password);

            cout << "Currency (1=USD,2=SGD,3=EUR): ";
            cin >> currency;

            cout << "Initial Balance: ";
            cin >> balance;
            cin.ignore();

            appendString(payload, name);
            appendString(payload, password);
            appendInt(payload, currency);
            appendFloat(payload, balance);
        }
        else if (opCode == OP_DEPOSIT || opCode == OP_WITHDRAW)
        {

            string name;
            uint32_t accNo;
            string password;
            uint32_t currency;
            float amount;

            cout << "Name: ";
            getline(cin, name);

            cout << "Account Number: ";
            if (!readUint32(accNo))
            {
                cout << "Invalid input. Account number must be a number.\n";
                continue;
            }

            cout << "Password: ";
            getline(cin, password);

            cout << "Currency (1=USD,2=SGD,3=EUR): ";
            if (!readUint32(currency))
            {
                cout << "Invalid input. Currency must be 1, 2, or 3.\n";
                continue;
            }

            cout << "Amount: ";
            if (!readFloatSafe(amount))
            {
                cout << "Invalid input. Amount must be a number.\n";
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

            string name;
            uint32_t accNo;
            string password;

            cout << "Name: ";
            getline(cin, name);

            cout << "Account Number: ";
            if (!readUint32(accNo))
            {
                cout << "Invalid input. Account number must be a number.\n";
                continue;
            }

            cout << "Password: ";
            getline(cin, password);

            appendString(payload, name);
            appendInt(payload, accNo);
            appendString(payload, password);
        }
        else if (opCode == OP_REGISTER_MONITOR)
        {

            cout << "Monitor interval (seconds): ";
            if (!readUint32(monitorSeconds))
            {
                cout << "Invalid input. Interval must be a number.\n";
                continue;
            }

            appendInt(payload, monitorSeconds);
        }
        else if (opCode == OP_CHECK_HISTORY)
        {

            string name;
            uint32_t accNo;
            string password;

            cout << "Name: ";
            getline(cin, name);

            cout << "Account Number: ";
            if (!readUint32(accNo))
            {
                cout << "Invalid input. Account number must be a number.\n";
                continue;
            }

            cout << "Password: ";
            getline(cin, password);

            appendString(payload, name);
            appendInt(payload, accNo);
            appendString(payload, password);
        }
        else if (opCode == OP_TRANSFER)
        {

            string name;
            uint32_t srcAccNo;
            string password;
            uint32_t currency;
            float amount;
            uint32_t destAccNo;

            cout << "Your Name: ";
            getline(cin, name);

            cout << "Your Account Number: ";
            if (!readUint32(srcAccNo))
            {
                cout << "Invalid input. Account number must be a number.\n";
                continue;
            }

            cout << "Password: ";
            getline(cin, password);

            cout << "Currency (1=USD,2=SGD,3=EUR): ";
            if (!readUint32(currency))
            {
                cout << "Invalid input. Currency must be 1, 2, or 3.\n";
                continue;
            }

            cout << "Amount to Transfer: ";
            if (!readFloatSafe(amount))
            {
                cout << "Invalid input. Amount must be a number.\n";
                continue;
            }

            cout << "Destination Account Number: ";
            if (!readUint32(destAccNo))
            {
                cout << "Invalid input. Account number must be a number.\n";
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
            cout << "Invalid option\n";
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
                cout << "[RETRY " << retries << "/" << MAX_RETRIES
                     << "] Resending request (requestId: " << currentRequestId << ")\n";
            }

            // Simulate request loss on client side
            bool dropSend = false;
            if (lossProbability > 0.0f)
            {
                float roll = static_cast<float>(rand()) / RAND_MAX;
                cout << "[SIMULATED LOSS] roll = " << roll << endl;
                if (roll < lossProbability)
                {
                    cout << "[SIMULATED LOSS] Dropping outgoing request\n";
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

                cout << "[DEBUG] Sending to "
                     << serverIP << ":" << port
                     << " | bytes: " << sent
                     << endl;

#ifdef _WIN32
                if (sent == SOCKET_ERROR)
                {
                    cout << "WSA Error: " << WSAGetLastError() << endl;
                }
#endif
            }

            int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, NULL, NULL);

            if (n < 0)
            {
                retries++;
                if (retries > MAX_RETRIES)
                {
                    cout << "No response after " << MAX_RETRIES << " retries. Giving up.\n";
                }
                else
                {
                    cout << "No response (timeout).\n";
                }
                continue;
            }

            vector<uint8_t> response(buffer, buffer + n);
            size_t offset = 0;

            uint32_t respClientId = readInt(response, offset);
            readInt(response, offset); // requestId
            uint32_t status = readInt(response, offset);
            readInt(response, offset); // payloadLen

            if (respClientId != clientId)
            {
                cout << "Received packet for different client\n";
                continue;
            }

            gotResponse = true;

            if (status == STATUS_SUCCESS)
            {

                if (opCode == OP_OPEN_ACCOUNT)
                {
                    uint32_t accNo = readInt(response, offset);
                    cout << "Account Created! Number: " << accNo << "\n";
                }
                else if (opCode == OP_DEPOSIT || opCode == OP_WITHDRAW)
                {
                    float newBalance = readFloat(response, offset);
                    cout << "New Balance: " << newBalance << "\n";
                }
                else if (opCode == OP_CLOSE_ACCOUNT)
                {
                    cout << "Account successfully closed.\n";
                }
                else if (opCode == OP_CHECK_HISTORY)
                {
                    uint32_t numTransactions = readInt(response, offset);
                    cout << "\n--- Transaction History (" << numTransactions << " entries) ---\n";
                    float currentBalance = 0.0f;
                    for (uint32_t i = 0; i < numTransactions; i++)
                    {
                        uint32_t op = readInt(response, offset);
                        float amount = readFloat(response, offset);
                        float balAfter = readFloat(response, offset);
                        string desc = readString(response, offset);
                        currentBalance = balAfter;

                        string opStr;
                        const char *color = "";
                        const char *reset = "";
                        string amountLabel = "Amount";
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

                        cout << "  " << (i + 1) << ". " << color << opStr << reset
                             << " | " << amountLabel << ": $"
                             << fixed << setprecision(2) << amount
                             << " | Balance After: $"
                             << fixed << setprecision(2) << balAfter
                             << " | " << desc << "\n";
                    }

                    if (numTransactions > 0)
                    {
                        cout << "Current Balance: $"
                             << fixed << setprecision(2)
                             << currentBalance << "\n";
                    }
                    else
                    {
                        cout << "Current Balance: $0.00 (no transactions yet)\n";
                    }

                    cout << "--- End of History ---\n";
                }
                else if (opCode == OP_TRANSFER)
                {
                    float srcBalance = readFloat(response, offset);
                    readFloat(response, offset); // skip destination balance
                    cout << "Transfer successful!\n";
                    cout << "  Your new balance: " << srcBalance << "\n";
                }
                else if (opCode == OP_REGISTER_MONITOR)
                {

                    cout << "Monitoring started...\n";
                    auto start = steady_clock::now();

                    while (true)
                    {

                        int n2 = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, NULL, NULL);

                        if (n2 > 0)
                        {

                            vector<uint8_t> msg(buffer, buffer + n2);
                            size_t offset2 = 0;

                            readInt(msg, offset2); // clientId
                            readInt(msg, offset2); // requestId
                            readInt(msg, offset2); // status
                            readInt(msg, offset2); // payloadLen

                            string eventLog = readString(msg, offset2);
                            cout << eventLog << "\n";
                        }

                        if (steady_clock::now() - start > seconds(monitorSeconds))
                        {
                            break;
                        }
                    }

                    cout << "Monitoring ended.\n";
                }
            }
            else
            {
                string errorMsg = readString(response, offset);
                cout << "Error: " << errorMsg << "\n";
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
