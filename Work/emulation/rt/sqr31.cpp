// src/rt/sqr31.cpp
#include "sqr31.hpp"
#include "../types/PLIError.hpp"
#include <cmath>

namespace rt {

static constexpr uint32_t ERR_SQRT_NEGATIVE=0x00011628;

double sqr31_1(types::Context& ctx, double value) {
  if(value<0.0)
    throw types::PLIError(ERR_SQRT_NEGATIVE);
  return std::sqrt(value);
}

} // namespace rt
