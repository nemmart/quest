// src/hw/EagleIntegration.cpp
#include "EagleIntegration.hpp"
#include "../debug/SymbolTable.hpp"
#include "../debug/CallStack.hpp"

namespace hw {

EagleIntegration::EagleIntegration(Machine& m) : machine(m) {
  machine.wide_push(machine.ac[0]);
  machine.wide_push(machine.ac[1]);
  machine.wide_push(machine.ac[2]);
  machine.wide_push(machine.wfp);
  machine.wide_push(machine.ac[3] | (machine.c<<31));
  fp=static_cast<uint32_t>(machine.wsp);
  machine.ac[3]=static_cast<int32_t>(fp);
  machine.wfp=static_cast<int32_t>(fp);
  machine.call_stack->augment(static_cast<int32_t>(fp), 0);
}

int EagleIntegration::arg_count() const {
  uint32_t slot=Machine::copy_segment(fp, fp-9);
  return static_cast<int>(machine.memory->read_word(slot)&0x7FFF);
}

uint32_t EagleIntegration::arg_addr(int n) const {
  uint32_t slot=Machine::copy_segment(fp, fp-10-2*n);
  return static_cast<uint32_t>(machine.memory->read_wide(slot));
}

uint32_t EagleIntegration::arg_wide(int n) const {
  return static_cast<uint32_t>(machine.memory->read_wide(arg_addr(n)));
}

uint32_t EagleIntegration::wrtn(uint32_t result) {
  uint32_t ac0_slot=Machine::copy_segment(fp, fp-8);
  int32_t ret_val, psr_args;
  uint32_t return_addr;

  machine.memory->write_wide(ac0_slot, static_cast<int32_t>(result));
  machine.wsp=machine.wfp;
  ret_val=machine.wide_pop();
  machine.wfp=machine.wide_pop();
  machine.ac[2]=machine.wide_pop();
  machine.ac[1]=machine.wide_pop();
  machine.ac[0]=machine.wide_pop();
  psr_args=machine.wide_pop();
  machine.ac[3]=machine.wfp;
  machine.set_psr(static_cast<uint32_t>(psr_args)>>16);
  machine.wsp-=2*(psr_args&0x7FFF);
  machine.c=static_cast<uint32_t>(ret_val)>>31;

  return_addr=static_cast<uint32_t>(ret_val)&0x7FFFFFFF;
  machine.call_stack->call_return(static_cast<int32_t>(return_addr));
  return return_addr;
}

uint32_t EagleIntegration::wrtn_void() {
  int32_t ret_val, psr_args;
  uint32_t return_addr;

  machine.wsp=machine.wfp;
  ret_val=machine.wide_pop();
  machine.wfp=machine.wide_pop();
  machine.ac[2]=machine.wide_pop();
  machine.ac[1]=machine.wide_pop();
  machine.ac[0]=machine.wide_pop();
  psr_args=machine.wide_pop();
  machine.ac[3]=machine.wfp;
  machine.set_psr(static_cast<uint32_t>(psr_args)>>16);
  machine.wsp-=2*(psr_args&0x7FFF);
  machine.c=static_cast<uint32_t>(ret_val)>>31;

  return_addr=static_cast<uint32_t>(ret_val)&0x7FFFFFFF;
  machine.call_stack->call_return(static_cast<int32_t>(return_addr));
  return return_addr;
}

uint32_t EagleIntegration::throw_lib_error(uint32_t signal_code) {
  uint32_t lib_error_addr=machine.symbols->address_for_name("?LIB_ERROR");
  uint32_t code_addr, our_ret;

  if(lib_error_addr==0xFFFFFFFF)
    throw std::runtime_error("EagleIntegration: ?LIB_ERROR not found in symbol table");

  machine.wide_push(static_cast<int32_t>(signal_code));
  code_addr=Machine::copy_segment(fp, static_cast<uint32_t>(machine.wsp));
  machine.wide_push(static_cast<int32_t>(code_addr));
  our_ret=static_cast<uint32_t>(
    machine.memory->read_wide(Machine::copy_segment(fp, fp)))&0x7FFFFFFF;
  machine.wide_push((machine.get_psr()<<16)|1);
  machine.ac[3]=static_cast<int32_t>(our_ret);

  machine.call_stack->call(static_cast<int32_t>(lib_error_addr),
    machine.ac[3], -1, 1);

  return lib_error_addr;
}

} // namespace hw
