#pragma once
#include <vector>
#include <string>
#include <cstdint>

void appendInt(std::vector<uint8_t>& buffer, uint32_t value);
void appendFloat(std::vector<uint8_t>& buffer, float value);
void appendString(std::vector<uint8_t>& buffer, const std::string& str);

uint32_t readInt(const std::vector<uint8_t>& buffer, size_t& offset);
float readFloat(const std::vector<uint8_t>& buffer, size_t& offset);
std::string readString(const std::vector<uint8_t>& buffer, size_t& offset);