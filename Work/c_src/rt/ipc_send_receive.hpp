// src/rt/ipc_send_receive.hpp
#pragma once
#include <cstdint>

namespace types { class Context; class WordArray; }

namespace rt {

// ISEND (SYSCALL 025) — fire and forget
int32_t ipc_send(types::Context& ctx,
                 int32_t destination_port,
                 int32_t origin_port,
                 int32_t user_flags,
                 const types::WordArray& send_data,
                 int32_t value);

// IREC (SYSCALL 026) — blocking receive
int32_t ipc_receive(types::Context& ctx,
                    int32_t& origin,
                    int32_t& destination_port,
                    int32_t& user_flags,
                    types::WordArray& receive_data,
                    int32_t& value);

// ISR (SYSCALL 0142) — send then receive
int32_t ipc_send_receive(types::Context& ctx,
                         int32_t destination_port,
                         int32_t origin_port,
                         int32_t& user_flags,
                         const types::WordArray& send_data,
                         int32_t send_value,
                         types::WordArray& receive_data,
                         int32_t& receive_value);

} // namespace rt
