#include "MemoryDumper.hpp"
#include "../hw/ReadWrite.hpp"
#include <cstdio>




namespace debug {
using namespace hw;

int MemoryDumper::count(ReadWrite& memory, uint32_t start, uint32_t stop) {
  uint32_t index;
  for (index=start; index<stop; index++) {
    if (memory.read_word(index) != 0)
      break;
  }
  return static_cast<int>(index - start);
}

void MemoryDumper::dump(ReadWrite& memory, uint32_t start, uint32_t stop) {
  static const char to_hex[] = "0123456789ABCDEF";
  uint32_t index = start;

  while (index < stop) {
    int zeros = count(memory, index, stop);
    if (zeros > 48) {
      zeros = zeros - zeros%8;
      std::printf("%06X    0000 0000 0000 0000 0000 0000 0000 0000  [                ]\n", index);
      std::printf("%06X    0000 0000 0000 0000 0000 0000 0000 0000  [                ]\n", index+8);
      std::puts("*");
      std::puts("*");
      std::printf("%06X    0000 0000 0000 0000 0000 0000 0000 0000  [                ]\n", index+zeros-8);
      index += zeros;
    }
    else {
      char line[60];
      for (int i=0; i<58; i++) line[i] = ' ';
      line[41] = '[';
      line[58] = ']';
      line[59] = '\0';

      for (uint32_t current=0; current<8 && index+current<stop; current++) {
        uint32_t word = memory.read_word(index+current);
        uint32_t byte1 = (word>>8)&0xFF, byte2 = word&0xFF;
        line[current*5+0] = to_hex[(word>>12)&0x0F];
        line[current*5+1] = to_hex[(word>>8)&0x0F];
        line[current*5+2] = to_hex[(word>>4)&0x0F];
        line[current*5+3] = to_hex[word&0x0F];
        if (byte1>32 && byte1<127) line[current*2+42] = static_cast<char>(byte1);
        if (byte2>32 && byte2<127) line[current*2+43] = static_cast<char>(byte2);
      }
      std::printf("%06X    %s\n", index, line);
      index += 8;
    }
  }
}

} // namespace debug
