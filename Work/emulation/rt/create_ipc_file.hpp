// src/rt/create_ipc_file.hpp
#pragma once
#include <cstdint>

namespace types { class Context; class String; }

namespace rt {

// ?CREATE_IPC_FILE — creates an IPC port file via SYSCALL CREATE.
void create_ipc_file_3(types::Context& ctx, const types::String& filename,
                       int32_t local_port, int32_t ignore3);

} // namespace rt
