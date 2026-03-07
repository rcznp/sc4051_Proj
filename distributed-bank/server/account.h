#pragma once
#include <string>
#include <vector>
#include "../common/protocol.h"

struct Transaction {
    uint32_t operation;    // OP_DEPOSIT, OP_WITHDRAW, OP_TRANSFER, etc.
    float amount;
    float balanceAfter;
    std::string description;
};

struct Account {
    uint32_t accountNumber;
    std::string name;
    std::string password;
    CurrencyType currency;
    float balance;
    std::vector<Transaction> history;
};
