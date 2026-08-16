// src/runtime/o_area.cpp
//
// Derivation: docs/Project4/DERIVATION.md §2. Body-for-body the T?AREA
// shape (runtime/t_area.cpp) with displacement -0x40 instead of -0x29:
//   7017fc39 WSAVS 0x0000;            frame, no locals (ovk=1)
//   7017fc3b LDASB 2;                 ac2 = wsb
//   7017fc3c XLEF 0,[ac2+0x7FC0];     ac0 = wsb - 0x40   (15-bit disp)
//   7017fc3e XWSTA 0,[ac3+0x7FF8];    saved-ac0 slot = ac0
//   7017fc40 WRTN;
// Footprint: the five-wide WSAVS image with the ac0 slot patched; the
// body's ac2 clobber (LDASB) is restored by WRTN from the unpatched
// slot, so entry ac1/ac2/carry all survive — identical to T?AREA.
#include "o_area.hpp"
#include "../hw/Machine.hpp"
#include "../hw/RTBridge.hpp"
#include "../hw/RTStubs.hpp"
#include "../debug/Capture.hpp"

namespace rt {
uint32_t o_area(hw::Machine& machine) {
  return static_cast<uint32_t>(machine.wsb) - 0x40;
}
}

namespace emu_rt {

uint32_t oq_area(hw::Machine& machine) {
  if(machine.rt_pending_return != 0) {
    // Nested-in-fallback guard (SharedProtocol.md): inside a fallback
    // span — today that is EVERY live path, since both callers sit in
    // the DEF?ON/?FATAL terminal subtree, which the clone only reaches
    // emulated. Emulate without re-arming.
    hw::RTStubs::log_call(machine, "O?AREA", "(native-skip: inside fallback span)");
    return hw::RTStubs::entry_address("O?AREA");
  }

  hw::RTBridge bridge(machine);

  if(bridge.arg_count()!=0) {
    // Never observed (both call sites are LCALL [..],0); symmetric
    // emulation per METHOD.md §12.
    hw::RTStubs::log_call(machine, "O?AREA", "(native-fallback: argc)");
    machine.rt_pending_return = static_cast<uint32_t>(machine.ac[3]);
    return hw::RTStubs::entry_address("O?AREA");
  }

  hw::RTStubs::log_call(machine, "O?AREA", "(native)");
  bridge.set_return_ac(0, static_cast<int32_t>(rt::o_area(machine)));
  bridge.emulate_frame();
  debug::Capture::native_footprint(machine);
  return bridge.native_return();
}

} // namespace emu_rt
