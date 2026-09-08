// src/runtime/udiv32.cpp
#include "udiv32.hpp"
#include "../hw/Machine.hpp"
#include "../hw/RTBridge.hpp"
#include "../hw/RTStubs.hpp"

namespace rt {

int32_t udiv32_3(int32_t dividend, int32_t divisor, int32_t& remainder, bool& error) {
  // Exact port of WDIVS (EagleCompute) with ac0=0, ac1=dividend, ac2=divisor.
  int64_t dst_long, src_long, rem;

  error=false;
  if(divisor==0) {
    error=true;
    remainder=0;         // escape: ac0 still 0
    return dividend;     // escape: ac1 still the dividend
  }
  dst_long=static_cast<int64_t>(static_cast<uint32_t>(dividend));
  src_long=divisor;
  rem=dst_long%src_long;
  dst_long=dst_long/src_long;
  if((dst_long>>31)!=0 && (dst_long>>31)!=-1) {
    error=true;
    remainder=0;
    return dividend;
  }
  remainder=static_cast<int32_t>(rem);
  return static_cast<int32_t>(dst_long);
}

} // namespace rt

namespace emu_rt {

uint32_t udiv32(hw::Machine& machine) {
  hw::RTBridge bridge(machine);
  int32_t dividend, divisor, remainder, quotient;
  bool error;

  if(bridge.arg_count()!=3) {
    hw::RTStubs::log_call(machine, "?UDIV32", "(native-fallback)");
    return hw::RTStubs::entry_address("?UDIV32");
  }

  hw::RTStubs::log_call(machine, "?UDIV32", "(native)");
  dividend=bridge.arg_wide(1);
  divisor=bridge.arg_wide(2);
  quotient=rt::udiv32_3(dividend, divisor, remainder, error);
  if(error)
    machine.ovr=1;
  bridge.set_arg_wide(3, remainder);
  bridge.set_return_ac(0, quotient);     // the body patches the saved-ac0 slot
  bridge.emulate_frame();                // WSAVS 0x0000: no locals, image only
  return bridge.native_return();
}

} // namespace emu_rt
