#pragma once
#include <cstdint>


namespace hw { class ReadWrite; }

namespace debug {
using namespace hw;


class MemoryDumper {
public:
  static int count(ReadWrite& memory, uint32_t start, uint32_t stop);
  static void dump(ReadWrite& memory, uint32_t start, uint32_t stop);
};

} // namespace debug
