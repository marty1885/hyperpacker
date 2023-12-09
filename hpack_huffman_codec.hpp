#pragma once

#include <cstdint>
#include <vector>
#include <string>

size_t hpack_huffman_encode(const char* src, size_t src_len, std::vector<uint8_t>& dst);
bool decode(const uint8_t* input, size_t input_bits, std::string& output);