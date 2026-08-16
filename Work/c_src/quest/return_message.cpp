// src/quest/return_message.cpp
#include "return_message.hpp"
#include "../types/Context.hpp"
#include "../types/OperatingSystem.hpp"

namespace quest {

void return_message(types::Context& ctx, const std::string& message) {
  ctx.os.terminate_process(message);
  // terminate_process is [[noreturn]] but the compiler can't see
  // through the virtual dispatch.
  __builtin_unreachable();
}

} // namespace quest
