// src/emu_quest/return_message.cpp
#include "return_message.hpp"
#include "../hw/EagleIntegration.hpp"
#include "../hw/Machine.hpp"
#include "../emu_types/VaryingString.hpp"
#include "../quest/return_message.hpp"
#include "../types/Context.hpp"

namespace emu_quest {
using namespace hw;

uint32_t return_message(Machine& machine) {
  EagleIntegration ei(machine);
  types::Context& ctx=*machine.native_context;
  std::string message;

  // arg3 is the message (CHAR VARYING).  If the pointer is null,
  // the original code uses a default 16-byte message from the runtime.
  // Since terminate_process just throws "EXIT!" regardless of message
  // content, we pass an empty string for the null-pointer case.
  uint32_t msg_addr=ei.arg_addr(3);
  if(msg_addr!=0) {
    emu_types::VaryingString vs(*machine.memory, msg_addr);
    message=vs.str();
  }

  // Never returns — terminate_process throws.
  quest::return_message(ctx, message);
}

} // namespace emu_quest
