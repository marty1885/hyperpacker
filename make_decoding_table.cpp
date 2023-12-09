#include "hpack_encode_table.h"

#include <iostream>
#include <map>
#include <cassert>

struct hpack_entry {
    uint8_t value;
    uint8_t postfix_bits;
    uint8_t keep_looking;
};

std::map<uint32_t, std::map<uint8_t, hpack_entry>> byte_prefix_map;

int main()
{
    for (int i = 0; i < 256; ++i)
    {
        auto& entry = hpack_encode_table[i];

        // grab the byte prefix (that is, the N-1 bytes of the encoding)
        uint32_t prefix = 0;
        auto code = entry.first;
        auto code_len = entry.second;
        auto prefix_len = code_len - (code_len % 8 ? code_len % 8 : 8);
        auto postfix_len = code_len % 8 ? code_len % 8 : 8;
        prefix = code >> postfix_len;

        uint8_t suffix = code & ((1 << postfix_len) - 1);
        if (entry.second % 8 != 0) {
            int shift = 8 - (entry.second % 8);
            suffix <<= shift;
            suffix >>= shift;
        }

        // add the prefix to the map
        auto& m = byte_prefix_map[prefix];
        if(m.find(suffix) != m.end())
        {
            std::cerr << "Duplicate suffix: " << std::hex << (uint32_t)suffix << std::dec << "\n";
            std::cerr << "    Prefix: " << std::hex << prefix << std::dec << "\n";
            std::cerr << "    Byte: " << (uint32_t)m[suffix].value << std::dec << "\n";
            std::cerr << "    New Byte: " << (uint32_t)i << std::dec << "\n";
            abort();
        }
        m[suffix] = { (uint8_t)i, (uint8_t)postfix_len, false };
    }

    // Walk the tables to patch in linking from prefix tables to downstream tables
    for(auto& [my_prefix, map] : byte_prefix_map) {
        if(my_prefix == 0)
            continue;

        auto upstream_prefix = my_prefix >> 8;
        assert(byte_prefix_map.find(upstream_prefix) != byte_prefix_map.end());
        auto& upstream_map = byte_prefix_map[upstream_prefix];
        upstream_map[my_prefix & 0xff] = { 0, 0, true };
    }

    std::cerr << "Total number of prefixes: " << byte_prefix_map.size() << "\n";

    std::cout << "#pragma once\n"
        << "#include <cstdint>\n"
        << "#include <vector>\n"
        << "#include <stdint.h>\n"
        << "#include <unistd.h>\n"
        << "\n"
        << "struct hpack_decoding_table_entry {\n"
        << "    uint8_t value;\n"
        << "    bool have_value;\n"
        << "    uint8_t postfix_bits;\n"
        << "};\n"
        << "\n"
        << "struct hpack_decoding_table {\n"
        << "    std::vector<hpack_decoding_table_entry> entries;\n"
        << "    uint32_t prefix;\n"
        << "    int8_t min_bits;\n"
        << "};\n"
        << "\n"
        << "static const hpack_decoding_table hpack_decode_table[] = {";

    for(auto& [key, map] : byte_prefix_map) {
        std::cerr << "Prefix: " << std::hex << key << std::dec << "\n";
        std::cout << " { {\n";

        std::vector<std::pair<uint8_t, bool>> entries;
        size_t max_entry_idx = 0;
        size_t min_bits = 8;
        std::map<uint8_t, hpack_entry> suffix_map;
        for(auto& [suffix, entry] : map) {
            auto& [value, postfix_bits, keeplooking] = entry;
            // std::cerr << "    Suffix: " << std::hex << (uint32_t)suffix << std::dec << " Byte: " << (uint32_t)value << ", virtual entry: " << (int)keeplooking << "\n";

            max_entry_idx = std::max(max_entry_idx, (size_t)suffix);
            if(!keeplooking)
                min_bits = std::min(min_bits, (size_t)postfix_bits);
            assert(suffix_map.find(suffix) == suffix_map.end());
            suffix_map[suffix] = entry;
        }
        assert(min_bits > 0);
        assert(max_entry_idx < 256 && max_entry_idx > 0);

        for(size_t i = 0; i <= max_entry_idx; ++i) {
            if(suffix_map.find(i) != suffix_map.end()) {
                auto& [value, postfix_bits, keeplooking] = suffix_map[i];
                if(keeplooking) {
                    std::cout << "  { 0, false, 0 },\n";
                    continue;
                }
                std::cout << "  { " << (uint32_t)value << ", true, " << (uint32_t)postfix_bits << " },";
            } else {
                std::cout << "  { 0, false, 0 },";
            }
            std::cout << "\n";
        }
        std::cout << " }, " << key << ", " << min_bits << " },\n";
    }
    std::cout << "};\n"
    << "\n"
    << "inline const hpack_decoding_table* hpack_get_decoding_table(uint32_t prefix)\n"
    << "{\n"
    << "    for(auto& entry : hpack_decode_table) {\n"
    << "        if(entry.prefix == prefix) {\n"
    << "            return &entry;\n"
    << "        }\n"
    << "    }\n"
    << "    return nullptr;\n"
    << "}\n";
}
