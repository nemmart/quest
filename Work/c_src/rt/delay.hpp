// src/rt/delay.hpp
#pragma once
#include <cstdint>

namespace types { class Context; }

namespace rt {

// ?DELAY — sleep for specified milliseconds
void delay_1(types::Context& ctx, int32_t milliseconds);

} // namespace rt
