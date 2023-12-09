match($0, /\s+(:?'.'|EOS)?\s*\(\s*([0-9]+)\)\s+\|([01\|]+)\s+\w+\s+\[\s*[0-9]+\].*/, arr){
    split(arr[3], a, "|")
    nbits = 0
    binstr = ""
    for(i in a)
        binstr = binstr a[i]
    nbits = length(binstr)
    binstr = "0b" binstr

    # Safety check I didsn't mess up the bitstring
    if(nbits > 32 || nbits == 0) {
        print "Internal Error: bitstring size is invalid"
        exit 1
    }
    if(symbol_idx == 256)
        symbol_idx = "EOS"
    printf("  {%s, %s}, // symbol %s\n", binstr, nbits, symbol_idx)
    symbol_idx++
}

BEGIN {
    print "#pragma once"
    print "#include <vector>"
    print "#include <utility>"
    print "#include <cstdint>"
    print ""
    print "static const std::vector<std::pair<uint32_t, int>> hpack_encode_table = {"

    symbol_idx = 0
}

END {
    print "};"
}
