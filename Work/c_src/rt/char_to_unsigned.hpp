// src/rt/char_to_unsigned.hpp
#pragma once
#include <cstdint>

namespace types { class Context; class String; }

namespace rt {

// ?CHAR_TO_UNSIGNED — 1-arg version (base 10)
uint32_t char_to_unsigned_1(types::Context& ctx, const types::String& str);

// ?CHAR_TO_UNSIGNED — 2-arg version (explicit base, 2-16)
uint32_t char_to_unsigned_2(types::Context& ctx, const types::String& str, int base);

} // namespace rt
