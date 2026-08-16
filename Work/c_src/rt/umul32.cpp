// src/rt/umul32.cpp
#include "umul32.hpp"

namespace rt {

uint32_t umul32_3(types::Context& ctx, uint32_t a, uint32_t b, uint16_t& overflow) {
  uint64_t result=static_cast<uint64_t>(a)*b;
  overflow=(result>0xFFFFFFFF) ? 1 : 0;
  return static_cast<uint32_t>(result);
}

} // namespace rt
