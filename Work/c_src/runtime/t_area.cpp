#include "t_area.hpp"
#include "../hw/Machine.hpp"
#include "../hw/RTBridge.hpp"
#include "../hw/RTStubs.hpp"
#include "../debug/Capture.hpp"
namespace rt {
uint32_t t_area(hw::Machine& machine) {
  return static_cast<uint32_t>(machine.wsb)-0x29;
}
}

namespace emu_rt {

// T?AREA (0x7017ED93, 8 words) — Project 3 wrapper.
//   WSAVS 0; LDASB 2; XLEF 0,[ac2-0x29]; XWSTA 0,[ac3-8]; WRTN
// [ac3-8] after WSAVS is the saved-ac0 slot (RTBridge frame image:
// ret|c at [wfp], wfp at -2, ac2 -4, ac1 -6, ac0 -8), so the body
// returns wsb-0x29 in ac0 via the slot-patch idiom — the ?UDIV32
// precedent exactly. WSAVS 0x0000: no locals; the footprint is the
// five-wide image with the patched ac0 slot. ac1/ac2/carry restored to
// entry values by WRTN (the body's LDASB 2 clobbers only the live ac2,
// which WRTN then restores from the unpatched slot).
uint32_t t_area(hw::Machine& machine) {
  hw::RTBridge bridge(machine);

  if(bridge.arg_count()!=0) {
    // Never observed (all call sites are LCALL [..],0); symmetric
    // emulation per METHOD.md sec. 12.
    hw::RTStubs::log_call(machine, "T?AREA", "(native-fallback: argc)");
    machine.rt_pending_return = static_cast<uint32_t>(machine.ac[3]);
    return hw::RTStubs::entry_address("T?AREA");
  }

  hw::RTStubs::log_call(machine, "T?AREA", "(native)");
  bridge.set_return_ac(0, static_cast<int32_t>(rt::t_area(machine)));
  bridge.emulate_frame();
  debug::Capture::native_footprint(machine);
  return bridge.native_return();
}

} // namespace emu_rt
