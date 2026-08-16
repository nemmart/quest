// src/rt/await_console_interrupt.hpp
#pragma once

namespace types { class Context; }

namespace rt {

// ?AWAIT_CONSOLE_INTERRUPT — block until console interrupt (Ctrl-C)
// Throws PLIError when interrupted.
void await_console_interrupt_0(types::Context& ctx);

} // namespace rt
