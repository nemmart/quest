// src/emu_rt/char_to_unsigned.cpp
#include "char_to_unsigned.hpp"
#include "../hw/EagleIntegration.hpp"
#include "../hw/Machine.hpp"
#include "../emu_types/VaryingString.hpp"
#include "../rt/char_to_unsigned.hpp"
#include "../types/PLIError.hpp"

namespace emu_rt {
using namespace hw;
using namespace types;

uint32_t char_to_unsigned(Machine& machine) {
  EagleIntegration ei(machine);
  try {
    types::Context& ctx=*machine.native_context;
    emu_types::VaryingString str;
    uint32_t base, result;
    int argc=ei.arg_count();

    if(argc==1) {
      str=emu_types::VaryingString(*machine.memory, ei.arg_addr(1));
      result=rt::char_to_unsigned_1(ctx, str);
    }
    else if(argc==2) {
      str=emu_types::VaryingString(*machine.memory, ei.arg_addr(1));
      base=machine.memory->read_word(ei.arg_addr(2))&0xFFFF;
      result=rt::char_to_unsigned_2(ctx, str, base);
    }
    else {
      throw std::runtime_error("char_to_unsigned: unsupported argument count");
    }

    return ei.wrtn(result);
  } catch(PLIError& e) {
    return ei.throw_lib_error(e.signal_code);
  }
}

} // namespace emu_rt
