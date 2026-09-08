// src/rt/close_file.hpp
#pragma once
#include <cstdint>

namespace types { class Context; }

namespace rt {

// ?CLOSE_FILE — close a file channel
// 1 LCALL arg: channel
void close_file_1(types::Context& ctx, int32_t channel);

} // namespace rt
