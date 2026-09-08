// src/rt/lookup_port.hpp
#pragma once
#include <cstdint>

namespace types { class Context; class String; }

namespace rt {

// ?LOOKUP_PORT — look up an IPC service by name
// arg1: service name string
// arg2: output — PID stored here
// arg3: unused (passed by caller but not read)
int32_t lookup_port_3(types::Context& ctx, const types::String& name, int32_t& output_pid, uint32_t ignore3);

} // namespace rt
