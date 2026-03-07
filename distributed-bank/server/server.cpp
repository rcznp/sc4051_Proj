#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <unordered_map>
#include "account.h"
#include "../common/protocol.h"
#include "../common/marshaller.h"
#include <chrono>
#include <algorithm>
#include <map>
#include <cstdlib>
#include <ctime>

// At-most-once: reply history keyed by (clientId, requestId)
std::map<std::pair<uint32_t,uint32_t>, std::vector<uint8_t>> replyHistory;
bool atMostOnce = false;
float lossProbability = 0.0f;
std::string opName(uint32_t op) {
    switch(op) {
        case OP_OPEN_ACCOUNT: return "OPEN";
        case OP_CLOSE_ACCOUNT: return "CLOSE";
        case OP_DEPOSIT: return "DEPOSIT";
        case OP_WITHDRAW: return "WITHDRAW";
        case OP_CHECK_BALANCE: return "CHECK_BALANCE";
        case OP_REGISTER_MONITOR: return "MONITOR";
        case OP_CHECK_HISTORY: return "CHECK_HISTORY";
        case OP_TRANSFER: return "TRANSFER";
        default: return "UNKNOWN";
    }
}
struct MonitorClient {
    sockaddr_in addr;
    std::chrono::steady_clock::time_point expiry;
};

std::vector<MonitorClient> monitors;
void notifyMonitors(int sockfd, const Account& acc, uint32_t operation) {

    //dynamic array of raw bytes
    std::vector<uint8_t> message;

    //build callback packet header
    appendInt(message, 0); // clientId = 0 (callback)
    appendInt(message, 0); //requestId = 0
    appendInt(message, STATUS_SUCCESS);

    //build callback packet payload
    std::vector<uint8_t> payload;
    appendInt(payload, operation);
    appendInt(payload, acc.accountNumber);
    appendFloat(payload, acc.balance);

    appendInt(message, payload.size());
    message.insert(message.end(),
                   payload.begin(),
                   payload.end());

    for (auto& m : monitors) {
        sendto(sockfd,
               message.data(),
               message.size(),
               0,
               (struct sockaddr*)&m.addr,
               sizeof(m.addr));
    }
}
std::unordered_map<uint32_t, Account> accounts;
uint32_t nextAccountNumber = 1000;
#define PORT 2222
#define BUFFER_SIZE 1024
void sendErrorReply(int sockfd,
                    uint32_t clientId,
                    uint32_t requestId,
                    const std::string& message,
                    struct sockaddr_in& clientAddr,
                    socklen_t addrLen) {

    std::vector<uint8_t> reply;
    appendInt(reply, clientId);
    appendInt(reply, requestId);
    appendInt(reply, STATUS_ERROR);

    std::vector<uint8_t> payload;
    appendString(payload, message);

    appendInt(reply, payload.size());
    reply.insert(reply.end(), payload.begin(), payload.end());
    std::cout << "[ERROR]"
          << " | clientId: " << clientId
          << " | requestId: " << requestId
          << " | message: " << message
          << std::endl;
    sendto(sockfd, reply.data(), reply.size(), 0,
           (struct sockaddr*)&clientAddr, addrLen);
}

// Send reply, cache it for at-most-once, and simulate reply loss
void sendReply(int sockfd,
               uint32_t clientId,
               uint32_t requestId,
               const std::vector<uint8_t>& reply,
               struct sockaddr_in& clientAddr,
               socklen_t addrLen) {

    // Cache reply for at-most-once
    if (atMostOnce) {
        replyHistory[std::make_pair(clientId, requestId)] = reply;
    }

    // Simulate reply loss
    if (lossProbability > 0.0f) {
        float roll = static_cast<float>(rand()) / RAND_MAX;
        if (roll < lossProbability) {
            std::cout << "[SIMULATED LOSS] Dropping reply to clientId: "
                      << clientId << " requestId: " << requestId << std::endl;
            return;
        }
    }

    sendto(sockfd, reply.data(), reply.size(), 0,
           (struct sockaddr*)&clientAddr, addrLen);
}

int main(int argc, char* argv[]) {
    int sockfd;
    char buffer[BUFFER_SIZE];
    //struct sockaddr_in defined in #include <netinet/in.h>
    struct sockaddr_in serverAddr, clientAddr;//represents an IPv4 address + port
    /*struct sockaddr_in {
    short sin_family;         // AF_INET (IPv4)
    unsigned short sin_port;  // Port number
    struct in_addr sin_addr;  // IP address
    char padding[8];          // Extra space
}; */
    if (argc < 3) {
        std::cerr << "Usage: ./server <port> <0=at-least-once|1=at-most-once> [loss_probability]\n";
        return 1;
    }

    int port = std::stoi(argv[1]);
    atMostOnce = (std::stoi(argv[2]) == 1);
    if (argc >= 4) {
        lossProbability = std::stof(argv[3]);
    }
    srand(time(nullptr));
    socklen_t addrLen = sizeof(clientAddr);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::cerr << "Socket creation failed\n";
        return 1;
    }
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);
    //bind() assigns IP address + Port number to socket
    //bind() expects: The socket, A pointer to struct sockaddr, The size of that structure
    //casting tells it treat this memory as a generic sockaddr.
    if (bind(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Bind failed\n";
        return 1;
    }

    std::cout << "UDP Server running on port " << port
              << " | Semantics: " << (atMostOnce ? "AT-MOST-ONCE" : "AT-LEAST-ONCE")
              << " | Loss probability: " << lossProbability
              << std::endl;

    while (true) {
        auto now = std::chrono::steady_clock::now();

        monitors.erase(
            std::remove_if(monitors.begin(), monitors.end(),
                [now](const MonitorClient& m) {
                    return now > m.expiry;
                }),
        monitors.end());
        int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0,
                         (struct sockaddr*)&clientAddr, &addrLen);
        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET,
                &clientAddr.sin_addr,
                clientIP,
                INET_ADDRSTRLEN);

        uint16_t clientPort = ntohs(clientAddr.sin_port);
        if (n <= 0) {
            std::cerr << "recvfrom failed\n";
            continue;
        }

        if (n < 16) {
            std::cerr << "Invalid packet size\n";
            continue;
        }

        std::vector<uint8_t> request(buffer, buffer + n);
        size_t offset = 0;

        uint32_t clientId     = readInt(request, offset);
        uint32_t requestId    = readInt(request, offset);
        uint32_t opCode       = readInt(request, offset);
        uint32_t payloadLength = readInt(request, offset);
        std::cout << "\n[REQUEST]"
          << " | IP: " << clientIP
          << ":" << clientPort
          << " | clientId: " << clientId
          << " | requestId: " << requestId
          << " | op: " << opName(opCode)
          << std::endl;

        if (payloadLength != request.size() - offset) {
                sendErrorReply(sockfd, clientId, requestId,
                    "Payload length mismatch",
                    clientAddr, addrLen);
                continue;
            }

        // Simulate request loss (drop the received packet before processing)
        if (lossProbability > 0.0f) {
            float roll = static_cast<float>(rand()) / RAND_MAX;
            if (roll < lossProbability) {
                std::cout << "[SIMULATED LOSS] Dropping request from clientId: "
                          << clientId << " requestId: " << requestId << std::endl;
                continue;
            }
        }

        // At-most-once: check if we already processed this request
        if (atMostOnce) {
            auto key = std::make_pair(clientId, requestId);
            auto histIt = replyHistory.find(key);
            if (histIt != replyHistory.end()) {
                std::cout << "[AT-MOST-ONCE] Duplicate request detected, returning cached reply"
                          << " | clientId: " << clientId
                          << " | requestId: " << requestId << std::endl;
                sendto(sockfd, histIt->second.data(), histIt->second.size(), 0,
                       (struct sockaddr*)&clientAddr, addrLen);
                continue;
            }
        }

        if (opCode == OP_OPEN_ACCOUNT) {

            try {
                std::string name = readString(request, offset);
                std::string password = readString(request, offset);
                uint32_t currencyVal = readInt(request, offset);
                float initialBalance = readFloat(request, offset);

                // Validation
                if (name.empty()) {
                    sendErrorReply(sockfd, clientId,requestId,
                                "Name cannot be empty",
                                clientAddr, addrLen);
                    continue;
                }

                if (password.length() < 4) {
                    sendErrorReply(sockfd, clientId,requestId,
                                "Password must be at least 4 characters",
                                clientAddr, addrLen);
                    continue;
                }

                if (initialBalance < 0) {
                    sendErrorReply(sockfd, clientId,requestId,
                                "Initial balance cannot be negative",
                                clientAddr, addrLen);
                    continue;
                }

                if (currencyVal < USD || currencyVal > EUR) {
                    sendErrorReply(sockfd, clientId,requestId,
                                "Invalid currency type",
                                clientAddr, addrLen);
                    continue;
                }

                // Create account
                Account acc;
                acc.accountNumber = nextAccountNumber++;
                acc.name = name;
                acc.password = password;
                acc.currency = static_cast<CurrencyType>(currencyVal);
                acc.balance = initialBalance;

                accounts[acc.accountNumber] = acc;

                // Build structured reply
                std::vector<uint8_t> reply;
                appendInt(reply, clientId);
                appendInt(reply, requestId);
                appendInt(reply, STATUS_SUCCESS);

                std::vector<uint8_t> payload;
                appendInt(payload, acc.accountNumber);

                appendInt(reply, payload.size());
                reply.insert(reply.end(), payload.begin(), payload.end());

                sendReply(sockfd, clientId, requestId, reply, clientAddr, addrLen);
                std::cout << "[SUCCESS]"
                    << " | clientId: " << clientId
                    << " | requestId: " << requestId
                    << " | Account created: "
                    << acc.accountNumber
                    << std::endl;
                notifyMonitors(sockfd, acc,OP_OPEN_ACCOUNT);

            } catch (...) {
                sendErrorReply(sockfd, clientId, requestId,
                            "Malformed request",
                            clientAddr, addrLen);
            }
        }
    else if (opCode == OP_DEPOSIT) {
        try {
            std::string name = readString(request, offset);
            uint32_t accountNumber = readInt(request, offset);
            std::string password = readString(request, offset);
            uint32_t currencyVal = readInt(request, offset);
            float amount = readFloat(request, offset);

            // Validate account exists
            auto it = accounts.find(accountNumber);
            if (it == accounts.end()) {
                sendErrorReply(sockfd, clientId,requestId,
                            "Account not found",
                            clientAddr, addrLen);
                continue;
            }
            //it ->(accountNumber, AccountObject)
            //it->first  → key (accountNumber)
            //it->second → value (Account object)
            Account& acc = it->second;

            // Validate name
            if (acc.name != name) {
                sendErrorReply(sockfd, clientId,requestId,
                            "Name does not match account holder",
                            clientAddr, addrLen);
                continue;
            }

            // Validate password
            if (acc.password != password) {
                sendErrorReply(sockfd, clientId,requestId,
                            "Invalid password",
                            clientAddr, addrLen);
                continue;
            }

            // Validate currency
            if (static_cast<CurrencyType>(currencyVal) != acc.currency) {
                sendErrorReply(sockfd, clientId,requestId,
                            "Currency type does not match account",
                            clientAddr, addrLen);
                continue;
            }

            // Validate amount
            if (amount <= 0) {
                sendErrorReply(sockfd, clientId,requestId,
                            "Deposit amount must be positive",
                            clientAddr, addrLen);
                continue;
            }

            // Perform deposit
            acc.balance += amount;

            // Record transaction
            acc.history.push_back({OP_DEPOSIT, amount, acc.balance,
                "Deposit of " + std::to_string(amount)});

            // Build structured reply
            std::vector<uint8_t> reply;
            appendInt(reply, clientId);
            appendInt(reply, requestId);
            appendInt(reply, STATUS_SUCCESS);

            std::vector<uint8_t> payload;
            appendFloat(payload, acc.balance);

            appendInt(reply, payload.size());
            reply.insert(reply.end(), payload.begin(), payload.end());

            sendReply(sockfd, clientId, requestId, reply, clientAddr, addrLen);

            std::cout << "[SUCCESS]"
                << " | clientId: " << clientId
                << " | requestId: " << requestId
                << " | Deposit new balance: "
                << acc.balance
                << std::endl;
            notifyMonitors(sockfd, acc,OP_DEPOSIT);

        } catch (...) {
            sendErrorReply(sockfd, clientId,requestId,
                        "Malformed deposit request",
                        clientAddr, addrLen);
        }
    }
    else if (opCode == OP_WITHDRAW) {
        try {
            std::string name = readString(request, offset);
            uint32_t accountNumber = readInt(request, offset);
            std::string password = readString(request, offset);
            uint32_t currencyVal = readInt(request, offset);
            float amount = readFloat(request, offset);

            // Validate account exists
            auto it = accounts.find(accountNumber);
            if (it == accounts.end()) {
                sendErrorReply(sockfd, clientId, requestId,
                            "Account not found",
                            clientAddr, addrLen);
                continue;
            }

            Account& acc = it->second;

            // Validate name
            if (acc.name != name) {
                sendErrorReply(sockfd, clientId, requestId,
                            "Name does not match account holder",
                            clientAddr, addrLen);
                continue;
            }

            // Validate password
            if (acc.password != password) {
                sendErrorReply(sockfd, clientId, requestId,
                            "Invalid password",
                            clientAddr, addrLen);
                continue;
            }

            // Validate currency
            if (static_cast<CurrencyType>(currencyVal) != acc.currency) {
                sendErrorReply(sockfd, clientId, requestId,
                            "Currency type does not match account",
                            clientAddr, addrLen);
                continue;
            }

            // Validate amount
            if (amount <= 0) {
                sendErrorReply(sockfd, clientId, requestId,
                            "Withdrawal amount must be positive",
                            clientAddr, addrLen);
                continue;
            }

            // Check sufficient balance
            if (acc.balance < amount) {
                sendErrorReply(sockfd, clientId,requestId,
                            "Insufficient funds",
                            clientAddr, addrLen);
                continue;
            }

            // Perform withdrawal
            acc.balance -= amount;

            // Record transaction
            acc.history.push_back({OP_WITHDRAW, amount, acc.balance,
                "Withdrawal of " + std::to_string(amount)});

            // Build success reply
            std::vector<uint8_t> reply;
            appendInt(reply, clientId);
            appendInt(reply, requestId);
            appendInt(reply, STATUS_SUCCESS);

            std::vector<uint8_t> payload;
            appendFloat(payload, acc.balance);

            appendInt(reply, payload.size());
            reply.insert(reply.end(), payload.begin(), payload.end());

            sendReply(sockfd, clientId, requestId, reply, clientAddr, addrLen);

            std::cout << "[SUCCESS]"
                << " | clientId: " << clientId
                << " | requestId: " << requestId
                << " | Withdraw new balance: "
                << acc.balance
                << std::endl;
            notifyMonitors(sockfd, acc,OP_WITHDRAW);

        } catch (...) {
            sendErrorReply(sockfd, clientId,requestId,
                        "Malformed withdrawal request",
                        clientAddr, addrLen);
        }
    }
    else if (opCode == OP_CLOSE_ACCOUNT) {
        try {
            std::string name = readString(request, offset);
            uint32_t accountNumber = readInt(request, offset);
            std::string password = readString(request, offset);

            auto it = accounts.find(accountNumber);
            if (it == accounts.end()) {
                sendErrorReply(sockfd, clientId,requestId,
                            "Account not found",
                            clientAddr, addrLen);
                continue;
            }

            Account& acc = it->second;

            // Validate name
            if (acc.name != name) {
                sendErrorReply(sockfd, clientId,requestId,
                            "Name does not match account holder",
                            clientAddr, addrLen);
                continue;
            }

            if (acc.password != password) {
                sendErrorReply(sockfd, clientId,requestId,
                            "Invalid password",
                            clientAddr, addrLen);
                continue;
            }

            if (acc.balance != 0.0f) {
                sendErrorReply(sockfd, clientId,requestId,
                            "Account balance must be zero to close",
                            clientAddr, addrLen);
                continue;
            }
            Account accCopy = acc;
            accounts.erase(it);
            notifyMonitors(sockfd, accCopy, OP_CLOSE_ACCOUNT);

            std::vector<uint8_t> reply;
            appendInt(reply, clientId);
            appendInt(reply, requestId);
            appendInt(reply, STATUS_SUCCESS);

            std::vector<uint8_t> payload; // empty
            appendInt(reply, payload.size());

            sendReply(sockfd, clientId, requestId, reply, clientAddr, addrLen);

            std::cout << "[SUCCESS]"
            << " | clientId: " << clientId
            << " | requestId: " << requestId
            << " | Account closed: "
            << accountNumber
            << std::endl;

        } catch (...) {
            sendErrorReply(sockfd, clientId, requestId,
                        "Malformed close request",
                        clientAddr, addrLen);
        }
    }
    else if (opCode == OP_REGISTER_MONITOR) {
            try {
                uint32_t intervalSeconds = readInt(request, offset);

                if (intervalSeconds == 0) {
                    sendErrorReply(sockfd, clientId, requestId,
                                "Interval must be > 0",
                                clientAddr, addrLen);
                    continue;
                }

                MonitorClient mc;
                mc.addr = clientAddr;
                mc.expiry = std::chrono::steady_clock::now()
                            + std::chrono::seconds(intervalSeconds);

                monitors.push_back(mc);

                std::vector<uint8_t> reply;
                appendInt(reply, clientId);
                appendInt(reply, requestId);
                appendInt(reply, STATUS_SUCCESS);

                std::vector<uint8_t> payload;
                appendInt(reply, payload.size());

                sendReply(sockfd, clientId, requestId, reply, clientAddr, addrLen);

                std::cout << "[SUCCESS]"
                    << " | clientId: " << clientId
                    << " | requestId: " << requestId
                    << " | Monitor registered for "
                    << intervalSeconds << " seconds"
                    << std::endl;

            } catch (...) {
                sendErrorReply(sockfd, clientId,requestId,
                            "Malformed monitor request",
                            clientAddr, addrLen);
            }
        }
    // ===== CHECK HISTORY (Idempotent) =====
    // Reading history does not modify any state, so calling it
    // multiple times always returns the same result.
    else if (opCode == OP_CHECK_HISTORY) {
        try {
            std::string name = readString(request, offset);
            uint32_t accountNumber = readInt(request, offset);
            std::string password = readString(request, offset);

            auto it = accounts.find(accountNumber);
            if (it == accounts.end()) {
                sendErrorReply(sockfd, clientId, requestId,
                            "Account not found",
                            clientAddr, addrLen);
                continue;
            }

            Account& acc = it->second;

            if (acc.name != name) {
                sendErrorReply(sockfd, clientId, requestId,
                            "Name does not match account holder",
                            clientAddr, addrLen);
                continue;
            }

            if (acc.password != password) {
                sendErrorReply(sockfd, clientId, requestId,
                            "Invalid password",
                            clientAddr, addrLen);
                continue;
            }

            // Build reply with transaction history
            std::vector<uint8_t> reply;
            appendInt(reply, clientId);
            appendInt(reply, requestId);
            appendInt(reply, STATUS_SUCCESS);

            std::vector<uint8_t> payload;
            appendInt(payload, static_cast<uint32_t>(acc.history.size()));
            for (auto& txn : acc.history) {
                appendInt(payload, txn.operation);
                appendFloat(payload, txn.amount);
                appendFloat(payload, txn.balanceAfter);
                appendString(payload, txn.description);
            }

            appendInt(reply, payload.size());
            reply.insert(reply.end(), payload.begin(), payload.end());

            sendReply(sockfd, clientId, requestId, reply, clientAddr, addrLen);

            std::cout << "[SUCCESS]"
                << " | clientId: " << clientId
                << " | requestId: " << requestId
                << " | History returned: "
                << acc.history.size() << " transactions"
                << std::endl;

        } catch (...) {
            sendErrorReply(sockfd, clientId, requestId,
                        "Malformed check history request",
                        clientAddr, addrLen);
        }
    }
    // ===== TRANSFER (Non-idempotent) =====
    // Each call moves money from one account to another,
    // so repeating the same request changes balances again.
    else if (opCode == OP_TRANSFER) {
        try {
            std::string name = readString(request, offset);
            uint32_t srcAccountNumber = readInt(request, offset);
            std::string password = readString(request, offset);
            uint32_t currencyVal = readInt(request, offset);
            float amount = readFloat(request, offset);
            uint32_t destAccountNumber = readInt(request, offset);

            // Validate source account
            auto srcIt = accounts.find(srcAccountNumber);
            if (srcIt == accounts.end()) {
                sendErrorReply(sockfd, clientId, requestId,
                            "Source account not found",
                            clientAddr, addrLen);
                continue;
            }

            Account& srcAcc = srcIt->second;

            if (srcAcc.name != name) {
                sendErrorReply(sockfd, clientId, requestId,
                            "Name does not match source account holder",
                            clientAddr, addrLen);
                continue;
            }

            if (srcAcc.password != password) {
                sendErrorReply(sockfd, clientId, requestId,
                            "Invalid password",
                            clientAddr, addrLen);
                continue;
            }

            if (static_cast<CurrencyType>(currencyVal) != srcAcc.currency) {
                sendErrorReply(sockfd, clientId, requestId,
                            "Currency type does not match source account",
                            clientAddr, addrLen);
                continue;
            }

            if (amount <= 0) {
                sendErrorReply(sockfd, clientId, requestId,
                            "Transfer amount must be positive",
                            clientAddr, addrLen);
                continue;
            }

            if (srcAcc.balance < amount) {
                sendErrorReply(sockfd, clientId, requestId,
                            "Insufficient funds in source account",
                            clientAddr, addrLen);
                continue;
            }

            // Validate destination account
            auto destIt = accounts.find(destAccountNumber);
            if (destIt == accounts.end()) {
                sendErrorReply(sockfd, clientId, requestId,
                            "Destination account not found",
                            clientAddr, addrLen);
                continue;
            }

            Account& destAcc = destIt->second;

            if (srcAcc.currency != destAcc.currency) {
                sendErrorReply(sockfd, clientId, requestId,
                            "Source and destination accounts must have the same currency",
                            clientAddr, addrLen);
                continue;
            }

            // Perform transfer
            srcAcc.balance -= amount;
            destAcc.balance += amount;

            // Record transaction history for both accounts
            srcAcc.history.push_back({OP_TRANSFER, amount, srcAcc.balance,
                "Transfer out to account " + std::to_string(destAccountNumber)});
            destAcc.history.push_back({OP_TRANSFER, amount, destAcc.balance,
                "Transfer in from account " + std::to_string(srcAccountNumber)});

            // Build reply: return both balances
            std::vector<uint8_t> reply;
            appendInt(reply, clientId);
            appendInt(reply, requestId);
            appendInt(reply, STATUS_SUCCESS);

            std::vector<uint8_t> payload;
            appendFloat(payload, srcAcc.balance);
            appendFloat(payload, destAcc.balance);

            appendInt(reply, payload.size());
            reply.insert(reply.end(), payload.begin(), payload.end());

            sendReply(sockfd, clientId, requestId, reply, clientAddr, addrLen);

            std::cout << "[SUCCESS]"
                << " | clientId: " << clientId
                << " | requestId: " << requestId
                << " | Transfer: " << amount
                << " from " << srcAccountNumber
                << " to " << destAccountNumber
                << " | Src balance: " << srcAcc.balance
                << " | Dest balance: " << destAcc.balance
                << std::endl;

            notifyMonitors(sockfd, srcAcc, OP_TRANSFER);
            notifyMonitors(sockfd, destAcc, OP_TRANSFER);

        } catch (...) {
            sendErrorReply(sockfd, clientId, requestId,
                        "Malformed transfer request",
                        clientAddr, addrLen);
        }
    }
    }

    close(sockfd);
    return 0;
}