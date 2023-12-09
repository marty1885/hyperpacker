#include "hpack_huffman_codec.hpp"
#include "hpack_encode_table.h"
#include "hpack_decode_table.h"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <algorithm>
#include <arpa/inet.h>

#include <iostream>
#include <math.h>

inline uint64_t htonll(uint64_t x)
{
    return ((uint64_t)htonl(x & 0xffffffff) << 32) | htonl(x >> 32);
}

size_t hpack_huffman_encode(const char* src, size_t src_len, std::vector<uint8_t>& dst)
{
    const size_t initial_size = dst.size();
    size_t accumulator = 0;
    size_t accumulator_bits = 0;
    constexpr size_t accumulator_max_bits = sizeof(accumulator) * 8;
    static_assert(accumulator_max_bits >= 30); // The max size HPACK Huffman code is 30 bits long

    for (size_t i = 0; i < src_len; ++i) {
        auto ch = src[i];
        auto& [code, nbits] = hpack_encode_table[ch];

        size_t remaining_bits = nbits;
        auto c = code;
        // This needs some explanation. The huffman code in HPACK at most 30 bits long. Able to be stored in 4 bytes.
        // Which happens to be the size of the accumulator on 32-bit systems. So the worst case scenario is that we
        // push 1 bit into the accumulator. Write it out. Then pus the remaining 29 bits into the accumulator. Under 
        // no circumstances can we need a loop to write out the accumulator. So we can just use a simple if statement
        // to decide if we need that write or not.
        // This optimization makes encoding 50% faster compared to looping. And few times faster then bit by bit
        if (remaining_bits <= accumulator_max_bits - accumulator_bits) {
            accumulator = (accumulator << remaining_bits) | c;
            accumulator_bits += remaining_bits;
            remaining_bits = 0;
        }
        else {
            size_t bits_to_copy = accumulator_max_bits - accumulator_bits;
            accumulator = (accumulator << bits_to_copy) | (c >> (remaining_bits - bits_to_copy));
            c = c & ((1 << (remaining_bits - bits_to_copy)) - 1);
            accumulator_bits += bits_to_copy;
            remaining_bits -= bits_to_copy;

            size_t old_size = dst.size();
            dst.resize(dst.size() + sizeof(accumulator));
            accumulator = htonll(accumulator);
            memcpy(dst.data() + old_size, &accumulator, sizeof(accumulator));
            // accumulator = 0; // will be written to later
            // accumulator_bits = 0;

            bits_to_copy = remaining_bits;
            accumulator = c;
            accumulator_bits = bits_to_copy;
            assert(remaining_bits == bits_to_copy);
            // remaining_bits -= bits_to_copy;
            remaining_bits = 0; // Must be 0
        }
    }

    const size_t increased = dst.size() - initial_size;

    if (accumulator_bits > 0) {
        const auto bytes = (accumulator_bits + 7) / 8;
        dst.resize(dst.size() + bytes);
        accumulator = htonll(accumulator << (accumulator_max_bits - accumulator_bits));
        memcpy(&dst[dst.size() - bytes], &accumulator, bytes);
    }

    return increased*8 + accumulator_bits;
}

bool decode(const uint8_t* input, size_t input_bits, std::string& output) {
    size_t read_pos_bits = 0;
    size_t decode_bits = 0;
    const hpack_decoding_table* table = hpack_get_decoding_table(0);
    const hpack_decoding_table* root_table = table;
    assert(table != nullptr);
    uint8_t current_byte = 0;
    uint32_t current_prefix = 0;

    if(input_bits < table->min_bits)
        return true; // RFC: Any unfinished Huffman-encoded string MUST be ignored

    while(read_pos_bits < input_bits) {
        size_t byte_pos = read_pos_bits / 8;
        size_t bit_pos = read_pos_bits % 8;
        // attempt to load as much data as possible, if we are at the beginning of a new byte of code
        if(decode_bits == 0) {
            size_t to_read = std::min((size_t)table->min_bits, 8 - bit_pos);
            auto rbyte = input[byte_pos];
            if(to_read + read_pos_bits > input_bits)
                return true;
            current_byte = rbyte >> (8 - to_read - bit_pos) & ((1 << to_read) - 1);
            read_pos_bits += to_read; 
            decode_bits += to_read;
        }
        // OK, if we are at byte boundary, we load partial data
        else if(decode_bits < table->min_bits - 1 && bit_pos != 7) {
            size_t to_read = std::min((size_t)table->min_bits - decode_bits, 8 - bit_pos);
            auto rbyte = input[byte_pos];
            auto mask = rbyte >> (8 - to_read - bit_pos) & ((1 << to_read) - 1);
            if(to_read + read_pos_bits > input_bits)
                return true;
            current_byte = current_byte << to_read | mask;
            read_pos_bits += to_read; 
            decode_bits += to_read;
        }
        else {
            // read 1 bit and see if we can decode it
            auto rbyte = input[byte_pos];
            auto bit = (rbyte >> (7 - bit_pos)) & 0x1;
            current_byte = current_byte << 1 | bit;
            read_pos_bits++;
            decode_bits++;
        }

        // check if we can decode
        size_t nentries = table->entries.size();
        if(current_byte >= nentries) {
            // invalid decoding
            return false; 
        }
        auto& entry = table->entries[current_byte];
        if(entry.have_value && entry.postfix_bits == decode_bits) {
            output += entry.value;
            decode_bits = 0;
            current_byte = 0;
            table = root_table;
            continue;
        }
        else if (decode_bits == 8) {
            current_prefix = current_prefix << 8 | current_byte;
            current_byte = 0;
            decode_bits = 0;
            table = hpack_get_decoding_table(current_prefix);
            if(table == nullptr)
                return false;
        }
    }
    return true;
}