// src/rt/udiv32.cpp
#include "udiv32.hpp"
#include "../types/PLIError.hpp"

namespace rt {

uint32_t udiv32_3(types::Context& ctx, uint32_t dividend, uint32_t divisor, uint32_t& remainder) {
  if(divisor==0)
    throw types::PLIError(0x00011628);  // division by zero
  remainder=dividend%divisor;
  return dividend/divisor;
}

} // namespace rt
