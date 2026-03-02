#pragma once
#include <cstdint>

enum OperationCode {
    OP_OPEN_ACCOUNT = 1,
    OP_CLOSE_ACCOUNT = 2,
    OP_DEPOSIT = 3,
    OP_WITHDRAW = 4,
    OP_CHECK_BALANCE = 5,
    OP_REGISTER_MONITOR = 6
};

enum CurrencyType {
    USD = 1,
    SGD = 2,
    EUR = 3
};

enum StatusCode {
    STATUS_SUCCESS = 0,
    STATUS_ERROR = 1
};

