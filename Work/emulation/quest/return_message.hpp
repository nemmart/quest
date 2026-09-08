// src/quest/return_message.hpp
#pragma once
#include <string>

namespace types { class Context; }

namespace quest {

// RETURN_MESSAGE — terminate process with a message.
// 3 LCALL args: flag, port_flags, message (varying string)
// Never returns.
[[noreturn]] void return_message(types::Context& ctx, const std::string& message);

} // namespace quest
