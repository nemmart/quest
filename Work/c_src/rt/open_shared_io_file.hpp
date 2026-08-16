// src/rt/open_shared_io_file.hpp
#pragma once
#include <cstdint>

namespace types {
class Context;
class String;
}

namespace rt {

// ?OPEN_SHARED_IO_FILE — open a file for paged/shared I/O
void open_shared_io_file_5(types::Context& ctx, int32_t& channel,
                           const types::String& filename, int32_t read_only,
                           int32_t unused4, int32_t unused5);

} // namespace rt
