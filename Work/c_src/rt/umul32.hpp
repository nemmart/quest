// src/rt/umul32.hpp
#pragma once
#include <cstdint>

namespace types { class Context; }

namespace rt {

// ?UMUL32 — unsigned 32-bit multiply
// Returns low 32 bits of product, sets overflow flag if result > 32 bits
uint32_t umul32_3(types::Context& ctx, uint32_t a, uint32_t b, uint16_t& overflow);

} // namespace rt
