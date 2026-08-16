// src/rt/current_pid.hpp
#pragma once
#include <cstdint>

namespace types { class Context; }

namespace rt {

// ?CURRENT_PID — returns the current process ID
int32_t current_pid_0(types::Context& ctx);

} // namespace rt
