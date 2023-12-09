#include <benchmark/benchmark.h>

#include "hpack_huffman_codec.hpp"

std::string input = "Hello, World!";

static void BM_HuffmanEncode(benchmark::State& state) {
  std::string output;
  for (auto _ : state) {
    std::vector<unsigned char> out;
    hpack_huffman_encode(input.c_str(), input.size(), out);
  }
  state.SetBytesProcessed(state.iterations() * input.size());
  state.SetLabel("HuffmanEncode");
}

static void BM_HuffmanDecode(benchmark::State& state) {
  std::vector<unsigned char> encoded;
  hpack_huffman_encode(input.c_str(), input.size(), encoded);
  std::string output;
  for (auto _ : state) {
    std::string res;
    decode(encoded.data(), encoded.size() * 8, res);
    benchmark::DoNotOptimize(res);
  }
  state.SetBytesProcessed(state.iterations() * input.size());
  state.SetLabel("HuffmanDecode");
}

BENCHMARK(BM_HuffmanEncode);
BENCHMARK(BM_HuffmanDecode);

BENCHMARK_MAIN();
