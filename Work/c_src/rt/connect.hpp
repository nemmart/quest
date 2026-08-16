// src/rt/connect.hpp
#pragma once
#include <cstdint>

namespace types { class Context; }

namespace rt {

// ?CONNECT — establish IPC connection to another process
void connect_1(types::Context& ctx, int32_t pid);

} // namespace rt
