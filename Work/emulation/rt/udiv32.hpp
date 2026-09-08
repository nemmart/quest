// src/rt/udiv32.hpp
#pragma once
#include <cstdint>

namespace types { class Context; }

namespace rt {

// ?UDIV32 — unsigned 32-bit divide
// Returns quotient, stores remainder via reference
uint32_t udiv32_3(types::Context& ctx, uint32_t dividend, uint32_t divisor, uint32_t& remainder);

} // namespace rt
