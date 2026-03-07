# Distributed Banking System

## Setup

```bash
cd distributed-bank
```

---

## Server (compile and run in WSL / Mac)

### (1) Compile

**Linux / WSL**
```bash
g++ server/server.cpp common/marshaller.cpp -o server/server
```

**Mac**
```bash
clang++ server/server.cpp common/marshaller.cpp -o server/server
```

### (2) Run

```bash
./server/server <port> <semantics> [loss_probability]
```

- `<port>` - Port number (e.g., `2222`)
- `<semantics>` - `0` = at-least-once, `1` = at-most-once
- `[loss_probability]` - Optional, 0.0 to 1.0 (default: 0.0). Simulates dropping packets to test invocation semantics.

**Examples:**

```bash
# At-least-once, no simulated loss
./server/server 2222 0

# At-most-once, 30% simulated packet loss
./server/server 2222 1 0.3
```

---

## Client

### (1) Compile

**Windows**
```bash
g++ client/client.cpp common/marshaller.cpp -o client.exe -lws2_32
```

**Linux / WSL**
```bash
g++ client/client.cpp common/marshaller.cpp -o client/client
```

**Mac**
```bash
clang++ client/client.cpp common/marshaller.cpp -o client/client
```

### (2) Run

**Windows**
```bash
.\client.exe <server_ip> <port> [loss_probability]
```

**Mac / Linux / WSL**
```bash
./client/client <server_ip> <port> [loss_probability]
```

- `<server_ip>` - IP address of the server (use the WSL IP if running server in WSL)
- `<port>` - Port number the server is listening on
- `[loss_probability]` - Optional, 0.0 to 1.0 (default: 0.0). Simulates dropping packets on client side.

**Examples:**

```bash
# Connect to server on localhost, no loss
.\client.exe 172.x.x.x 2222

# Connect with 20% simulated client-side loss
.\client.exe 172.x.x.x 2222 0.2
```

To find the WSL IP address, run this inside WSL:
```bash
hostname -I
```

---

## Menu Options

| # | Operation | Description |
|---|-----------|-------------|
| 1 | Open Account | Create a new account (name, password, currency) |
| 2 | Deposit | Deposit money into an account |
| 3 | Withdraw | Withdraw money from an account |
| 4 | Close Account | Delete an account |
| 5 | Monitor Updates | Register to receive real-time notifications for a duration |
| 6 | Check History | View transaction history (idempotent) |
| 7 | Transfer | Transfer money between accounts (non-idempotent) |
| 8 | Test Invocation Semantics | Send duplicate requests to observe at-least-once vs at-most-once behavior |
| 9 | Exit | Quit the client |

---

## Notes

- You can open multiple client terminals to test monitoring and concurrent access.
- The server must be restarted to switch between at-least-once and at-most-once semantics.
- Restarting the server clears all accounts and reply history.
- Simulated loss helps demonstrate the difference between the two invocation semantics — try option 8 with loss enabled.
