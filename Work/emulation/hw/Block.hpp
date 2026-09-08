#pragma once
#include "Page.hpp"
#include <cstring>


namespace hw {
struct Block {
  int32_t base = 0;
  Page* pages[512];

  Block() {
    std::memset(pages, 0, sizeof(pages));
  }
};

} // namespace hw
