#include "marshaller.h"
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <arpa/inet.h>
#endif
#include <cstring>

void appendInt(std::vector<uint8_t>& buffer, uint32_t value) {
    uint32_t netValue = htonl(value);
    uint8_t* ptr = reinterpret_cast<uint8_t*>(&netValue);
    buffer.insert(buffer.end(), ptr, ptr + sizeof(uint32_t));
}
 
void appendFloat(std::vector<uint8_t>& buffer, float value) {
    uint32_t asInt;
    std::memcpy(&asInt, &value, sizeof(float));
    appendInt(buffer, asInt);
}

void appendString(std::vector<uint8_t>& buffer, const std::string& str) {
    appendInt(buffer, static_cast<uint32_t>(str.length()));//Length-prefixed encoding.need to know length of string
    buffer.insert(buffer.end(), str.begin(), str.end());
}

uint32_t readInt(const std::vector<uint8_t>& buffer, size_t& offset) {
    uint32_t netValue;
    std::memcpy(&netValue, &buffer[offset], sizeof(uint32_t));
    offset += sizeof(uint32_t);
    //Network To Host Long
    //converts a 32-bit integer from network byte order into host machine’s native byte order
    return ntohl(netValue);
}

float readFloat(const std::vector<uint8_t>& buffer, size_t& offset) {
    uint32_t asInt = readInt(buffer, offset);
    float value;
    std::memcpy(&value, &asInt, sizeof(float));
    return value;
}

std::string readString(const std::vector<uint8_t>& buffer, size_t& offset) {
    uint32_t length = readInt(buffer, offset);
    std::string str(buffer.begin() + offset, buffer.begin() + offset + length);
    offset += length;
    return str;
}