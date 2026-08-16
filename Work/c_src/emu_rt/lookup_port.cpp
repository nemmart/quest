// src/emu_rt/lookup_port.cpp
#include "lookup_port.hpp"
#include "../hw/EagleIntegration.hpp"
#include "../hw/Machine.hpp"
#include "../emu_types/VaryingString.hpp"
#include "../rt/lookup_port.hpp"
#include "../types/PLIError.hpp"

namespace emu_rt {
using namespace hw;

uint32_t lookup_port(Machine& machine) {
  EagleIntegration ei(machine);
  try {
    types::Context& ctx=*machine.native_context;
    emu_types::VaryingString name;
    int32_t output_pid;
    uint32_t ignore3;
    int argc=ei.arg_count();

    if(argc==3) {
      name=emu_types::VaryingString(*machine.memory, ei.arg_addr(1));
      ignore3=ei.arg_wide(3);
      rt::lookup_port_3(ctx, name, output_pid, ignore3);
      machine.memory->write_wide(ei.arg_addr(2), output_pid);
    }
    else {
      throw std::runtime_error("lookup_port: unsupported argument count");
    }

    return ei.wrtn_void();
  } catch(types::PLIError& e) {
    return ei.throw_lib_error(e.signal_code);
  }
}

} // namespace emu_rt
