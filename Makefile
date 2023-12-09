ROOT_DIR:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
CXXFLAGS=-O2 -std=c++17 -DNDEBUG
# CXXFLAGS=-g -fsanitize=address -std=c++17

all: hpack_encode_table.h hpack_huffman_codec.o benchmark test hpack_decode_table.h

hpack_encode_table.h: $(ROOT_DIR)/hpack-rfc-huffman-table.txt $(ROOT_DIR)/make_encoding_table.awk
	gawk -f $(ROOT_DIR)/make_encoding_table.awk $(ROOT_DIR)/hpack-rfc-huffman-table.txt > $(ROOT_DIR)/hpack_encode_table.h

hpack_huffman_codec.o: $(ROOT_DIR)/hpack_huffman_codec.cpp $(ROOT_DIR)/hpack_huffman_codec.hpp hpack_encode_table.h hpack_decode_table.h
	g++ -c $(ROOT_DIR)/hpack_huffman_codec.cpp -o $(ROOT_DIR)/hpack_huffman_codec.o $(CXXFLAGS)

make_decoding_table: $(ROOT_DIR)/make_decoding_table.cpp $(ROOT_DIR)/hpack_huffman_codec.o
	g++ $(ROOT_DIR)/make_decoding_table.cpp $(ROOT_DIR)/hpack_huffman_codec.o -o $(ROOT_DIR)/make_decoding_table $(CXXFLAGS)

hpack_decode_table.h: $(ROOT_DIR)/make_decoding_table
	cd $(ROOT_DIR) && ./make_decoding_table > hpack_decode_table.h

benchmark: $(ROOT_DIR)/benchmark.cpp $(ROOT_DIR)/hpack_huffman_codec.o
	g++ $(ROOT_DIR)/benchmark.cpp $(ROOT_DIR)/hpack_huffman_codec.o -o $(ROOT_DIR)/benchmark -lbenchmark $(CXXFLAGS)

test: $(ROOT_DIR)/test.cpp $(ROOT_DIR)/hpack_huffman_codec.o
	g++ $(ROOT_DIR)/test.cpp $(ROOT_DIR)/hpack_huffman_codec.o -o $(ROOT_DIR)/test $(CXXFLAGS)

clean:
	rm -f $(ROOT_DIR)/hpack_encode_table.h
	rm -f $(ROOT_DIR)/hpack_huffman_codec.o
	rm -f $(ROOT_DIR)/benchmark
