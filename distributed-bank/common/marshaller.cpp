#include "marshaller.h"
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <arpa/inet.h>
#endif
#include <cstring>

using std::memcpy;
using std::string;
using std::vector;

void appendInt(vector<uint8_t>& buffer, uint32_t value) {
    uint32_t netValue = htonl(value);
    uint8_t* ptr = reinterpret_cast<uint8_t*>(&netValue);
    buffer.insert(buffer.end(), ptr, ptr + sizeof(uint32_t));
}
 
void appendFloat(vector<uint8_t>& buffer, float value) {
    uint32_t asInt;
    memcpy(&asInt, &value, sizeof(float));
    appendInt(buffer, asInt);
}

void appendString(vector<uint8_t>& buffer, const string& str) {
    appendInt(buffer, static_cast<uint32_t>(str.length()));//Length-prefixed encoding.need to know length of string
    buffer.insert(buffer.end(), str.begin(), str.end());
}

uint32_t readInt(const vector<uint8_t>& buffer, size_t& offset) {
    uint32_t netValue;
    memcpy(&netValue, &buffer[offset], sizeof(uint32_t));
    offset += sizeof(uint32_t);
    //Network To Host Long
    //converts a 32-bit integer from network byte order into host machine’s native byte order
    return ntohl(netValue);
}

float readFloat(const vector<uint8_t>& buffer, size_t& offset) {
    uint32_t asInt = readInt(buffer, offset);
    float value;
    memcpy(&value, &asInt, sizeof(float));
    return value;
}

string readString(const vector<uint8_t>& buffer, size_t& offset) {
    uint32_t length = readInt(buffer, offset);
    string str(buffer.begin() + offset, buffer.begin() + offset + length);
    offset += length;
    return str;
}
