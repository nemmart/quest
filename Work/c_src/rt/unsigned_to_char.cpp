// src/rt/unsigned_to_char.cpp
#include "unsigned_to_char.hpp"
#include "../types/String.hpp"
#include <cstdio>

namespace rt {

void unsigned_to_char_1(types::Context& ctx, types::String& dest, uint32_t value) {
  char buf[12];
  int len=snprintf(buf, sizeof(buf), "%u", value);
  dest.assign(std::string(buf, len));
}

} // namespace rt
