## Setup

```bash
cd distributed-bank
```

---

## Client

### (1) Compile

**Windows**
```bash
g++ client/client.cpp common/marshaller.cpp -o client.exe -lws2_32
```

**Mac**
```bash
clang++ client/client.cpp common/marshaller.cpp -o client/client
```

### (2) Run

**Windows**
```bash
.\client.exe <server ip address> 2222
```

---

## Server

### (1) Compile (Mac)

```bash
clang++ server/server.cpp common/marshaller.cpp -o server/server
```

### (2) Run

```bash
./server/server 2222
``` 
# Distributed Banking System 

## Packet format

| clientId (4 bytes)     |
| requestId (4 bytes)    |
| opCode / status (4B)   |
| payloadLength (4B)     |
| payload (variable)     |

## Request Packet
HEADER (16 bytes total)
--------------------------------
uint32 clientId
uint32 requestId
uint32 opCode
uint32 payloadLength

PAYLOAD
--------------------------------


## Reply Packet
uint32 clientId
uint32 requestId
uint32 status (STATUS_SUCCESS or STATUS_ERROR)
uint32 payloadLength
payload

## Monitor Callback Packet
clientId  = 0
requestId = 0
status    = STATUS_SUCCESS

payload:
    uint32 operation
    uint32 accountNumber
    float  balance



