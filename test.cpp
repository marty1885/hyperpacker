#include "hpack_huffman_codec.hpp"

#include <iostream>
#include <string>

int main() {
  std::string str = "Hello, World!";
  std::vector<uint8_t> encoded;
  size_t bits = hpack_huffman_encode(str.c_str(), str.size(), encoded);
  std::cout << "Encoded bits: " << bits << std::endl;
  std::cout << "Compression ratio: " << (bits / (str.size() * 8.0)) << std::endl;

  for (auto c : encoded) {
    printf("%02x ", c);
  }
  std::cout << std::endl;

  std::string decoded;
  bool ok = decode(encoded.data(), bits, decoded);
  if (!ok) {
    std::cout << "Decode failed" << std::endl;
    return 1;
  }
  std::cout << "Decoded: " << decoded << std::endl;

}
