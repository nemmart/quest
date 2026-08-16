// src/rt/ipc_send_receive.cpp
#include "ipc_send_receive.hpp"
#include "../types/Context.hpp"
#include "../types/OperatingSystem.hpp"

namespace rt {

int32_t ipc_send(types::Context& ctx,
                 int32_t destination_port,
                 int32_t origin_port,
                 int32_t user_flags,
                 const types::WordArray& send_data,
                 int32_t value) {
  return ctx.os.ipc_send(destination_port, origin_port, user_flags, send_data, value);
}

int32_t ipc_receive(types::Context& ctx,
                    int32_t& origin,
                    int32_t& destination_port,
                    int32_t& user_flags,
                    types::WordArray& receive_data,
                    int32_t& value) {
  return ctx.os.ipc_receive(origin, destination_port, user_flags, receive_data, value);
}

int32_t ipc_send_receive(types::Context& ctx,
                         int32_t destination_port,
                         int32_t origin_port,
                         int32_t& user_flags,
                         const types::WordArray& send_data,
                         int32_t send_value,
                         types::WordArray& receive_data,
                         int32_t& receive_value) {
  return ctx.os.ipc_send_receive(destination_port, origin_port, user_flags,
                                 send_data, send_value, receive_data, receive_value);
}

} // namespace rt
