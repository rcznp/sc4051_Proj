#pragma once
#include <string>
#include "../common/protocol.h"

struct Account {
    uint32_t accountNumber;
    std::string name;
    std::string password;
    CurrencyType currency;
    float balance;
};