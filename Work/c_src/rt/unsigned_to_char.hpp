// src/rt/unsigned_to_char.hpp
#pragma once
#include <cstdint>

namespace types {
class Context;
class String;
}

namespace rt {

// ?UNSIGNED_TO_CHAR — convert unsigned integer to decimal string
// 1 LCALL arg: value. Destination string passed via AC2 register.
void unsigned_to_char_1(types::Context& ctx, types::String& dest, uint32_t value);

} // namespace rt
