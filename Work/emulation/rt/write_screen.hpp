// src/rt/write_screen.hpp
#pragma once
#include <cstdint>

namespace types { class Context; class String; }

namespace rt {

// ?WRITE_SCREEN — 2-arg: write string to terminal channel
void write_screen_2(types::Context& ctx, int32_t channel, const types::String& text);

// ?WRITE_SCREEN — 5-arg: write string with cursor positioning
// options: 0x0800 (?ESCP) = position cursor, 0x0200 = write position back
void write_screen_5(types::Context& ctx, int32_t channel, const types::String& text,
                    int32_t& row, int32_t& col, int32_t options);

} // namespace rt
