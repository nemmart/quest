// src/rt/recreate_file.hpp
#pragma once
#include <cstdint>

namespace types { class Context; class String; }

namespace rt {

// ?RECREATE_FILE — delete and recreate a file via SYSCALL RECREATE.
void recreate_file_2(types::Context& ctx, const types::String& filename,
                     int32_t ignore2);

} // namespace rt
