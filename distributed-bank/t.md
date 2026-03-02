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


