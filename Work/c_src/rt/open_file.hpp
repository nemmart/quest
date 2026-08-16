// src/rt/open_file.hpp
#pragma once
#include <cstdint>

namespace types {
class Context;
class String;
}

namespace rt {

// ?OPEN_FILE — open a file and return channel number
// 2 LCALL args: &channel, &filename
void open_file_2(types::Context& ctx, int32_t& channel, const types::String& filename);

} // namespace rt
