// src/rt/read_screen.hpp
#pragma once
#include <cstdint>

namespace types {
class Context;
class String;
}

namespace rt {

// ?READ_SCREEN with 3 args — terminal line-mode read into VARYING string
void read_screen_3(types::Context& ctx, int32_t channel, types::String& result,
                   int32_t max_length);

} // namespace rt
