# Distributed Banking System – Current Features

## Core Infrastructure Implemented

- UDP client-server communication
- Custom binary request/reply protocol
- Manual marshalling (serialization)
- Manual unmarshalling (deserialization)
- Network byte-order handling (htonl / ntohl)
- Timeout handling on client (SO_RCVTIMEO)
- Defensive packet validation
- Structured error replies
- Offset-based sequential parsing

---

## Custom Protocol Design

### Request Format
[RequestID]
[OperationCode]
[PayloadLength]
[Payload]

### Reply Format
[RequestID]
[StatusCode]
[PayloadLength]
[Payload]

Status codes:
- STATUS_SUCCESS
- STATUS_ERROR

---

## Marshalling Utilities

Implemented:

- appendInt()
- appendFloat()
- appendString()
- readInt()
- readFloat()
- readString()

Key techniques:
- Length-prefixed string encoding
- Network byte order conversion
- Offset passed by reference for sequential parsing

---

## Banking Logic Implemented

- Open Account operation
- Account structure defined
- In-memory account storage using unordered_map
- Auto-increment account numbers

---

## Validation Added

Server:
- Minimum packet size check
- Payload length verification
- Input validation (name, password, currency, balance)
- Structured error responses

Client:
- Reply size validation
- Request ID verification
- Payload length validation
- Error message handling

---

## Project Structure

distributed-bank/
  client/
  server/
  common/

---

## Not Yet Implemented

- Deposit(done)
- Withdraw(done)
- Close Account
- Authentication verification for operations
- Monitor callback system
- Retry logic
- Duplicate request detection
- At-least-once semantics
- At-most-once semantics
- Packet loss simulation
- Menu-based client loop

---

Deposit operation (state mutation + authentication).