// src/rt/write_screen.cpp
#include "write_screen.hpp"
#include "../types/Context.hpp"
#include "../types/OperatingSystem.hpp"
#include "../types/PLIError.hpp"
#include "../types/String.hpp"

namespace rt {

void write_screen_2(types::Context& ctx, int32_t channel, const types::String& text) {
  int32_t err=ctx.os.write_screen(channel, text);
  if(err) throw types::PLIError(static_cast<uint32_t>(err));
}

void write_screen_5(types::Context& ctx, int32_t channel, const types::String& text,
                    int32_t& row, int32_t& col, int32_t options) {
  int32_t err=ctx.os.write_screen(channel, text, row, col, options);
  if(err) throw types::PLIError(static_cast<uint32_t>(err));
}

} // namespace rt
