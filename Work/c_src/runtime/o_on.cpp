// src/runtime/o_on.cpp
//
// Project 8 Stage A: the node search/registration/deactivation writes
// moved behind rt::error_handler_api (mv_error_handler.cpp holds the
// verbatim lifts). This file keeps the COMMON halves: the WSSVS/WSSVR
// images and the chain-search helper's residue (contract-private stack
// storage — Register E8, laid in every mode), the normative wsp
// effects, and the SS return staging. rt::chain_search below is
// mv-internal since Project 8 (no wrapper calls it).
#include "o_on.hpp"
#include "error_handler.hpp"
#include "../hw/Machine.hpp"
#include "../hw/RTBridge.hpp"
#include "../hw/RTStubs.hpp"
#include "../debug/Capture.hpp"
#include <cstring>

namespace rt {

void chain_search(hw::Memory& memory, int32_t frame, int32_t key1,
                  int32_t key2, ChainSearchResult& out) {
  int32_t node, head_slot;

  out.found=false;
  out.node=0;
  out.scratch=0;             // the WPSH 0,0 initial scratch value
  if(frame<=0)               // WSGT 2,2 guard on the frame argument
    return;
  head_slot=memory.read_wide(static_cast<uint32_t>(frame)+0xA);   // @[frame+0xA]: pointer to the head slot
  node=memory.read_wide(static_cast<uint32_t>(head_slot));
  while(node>0) {            // WSGT 2,2 guard per node
    int32_t type=memory.read_wide(static_cast<uint32_t>(node)+0x2);
    if(type==0)
      out.scratch=node;      // STATS: LAST zero-key node wins as backstop
    if(type==key1 &&
       memory.read_wide(static_cast<uint32_t>(node)+0x4)==key2) {
      out.found=true;
      out.node=node;
      return;
    }
    node=memory.read_wide(static_cast<uint32_t>(node)+0x0);
  }
  out.node=out.scratch;      // not found: result = backstop-or-0
}

} // namespace rt

namespace emu_rt {

// Shared residue writer: the chain-search helper's own WSSVR frame
// image, scratch wide, result patched into its saved-ac1 slot, and the
// (possibly ISZTS-incremented) return wide. helper_wsp = wsp at the
// XJSR; helper frame base = helper_wsp+12.
static void write_helper_residue(hw::Machine& machine, int32_t helper_wsp,
                                 int32_t helper_psr, int32_t key1,
                                 int32_t frame_arg, int32_t caller_frame_of_helper,
                                 uint32_t helper_ret, int32_t carry,
                                 const rt::ChainSearchResult& search) {
  uint32_t base=static_cast<uint32_t>(helper_wsp);
  int32_t ret_wide=static_cast<int32_t>(helper_ret | (static_cast<uint32_t>(carry)<<31));
  machine.memory->write_wide(base+2, helper_psr<<16);
  machine.memory->write_wide(base+4, key1);
  machine.memory->write_wide(base+6, search.node);   // saved-ac1 slot: overwritten with the result (found node or backstop-or-0)
  machine.memory->write_wide(base+8, frame_arg);
  machine.memory->write_wide(base+10, caller_frame_of_helper);
  machine.memory->write_wide(base+12, search.found ? ret_wide : ret_wide+1);      // ISZTS skip-return on not-found
  machine.memory->write_wide(base+14, search.scratch);                            // the WPSH scratch / STATS backstop slot
}

uint32_t o_on(hw::Machine& machine) {
  hw::RTBridge bridge(machine, hw::RTBridge::SS);
  int32_t type, raw_key2, handler, caller, wsp0, frame, final_wsp;

  hw::RTStubs::log_call(machine, "O.ON", "(native)");
  type=bridge.entry_ac(0);
  raw_key2=bridge.entry_ac(1);
  handler=bridge.entry_ac(2);
  caller=machine.wfp;                        // implicit argument: caller's frame
  wsp0=bridge.entry_wsp();
  frame=wsp0+12;                             // O.ON's would-be WSSVS frame pointer

  // Project 8 order contract with mv_error_handler::register_node:
  // the WSSVS image is laid FIRST (the allocate path's relocated image
  // and node overwrite parts of it, exactly as the emulated relocation
  // overwrites the real pushes); the node/relocation/relink writes run
  // inside the api call; the helper residue is laid AFTER (address-
  // disjoint from every register_node write — [frame+10..frame+22] vs
  // [frame-10..frame+9] — so net memory is identical to the emulated
  // order: image, residue, node).
  bridge.emulate_frame_ss();

  rt::RegisterIn in;
  rt::RegisterOutcome out;
  in.caller_frame=caller;
  in.type=type;
  in.raw_key2=raw_key2;
  in.handler=handler;
  in.entry_wsp=wsp0;
  in.entry_psr=bridge.entry_psr();
  in.entry_carry=bridge.entry_carry();
  in.entry_ac3=machine.ac[3];               // ac3 still holds the LJSR return
  rt::error_handler().register_node(machine, in, out);

  // The helper's residue (helper runs at wsp = frame+8; its pushed psr
  // carries ovk=1 from O.ON's WSSVS). Values from the outcome's
  // MV-RESIDUE fields.
  rt::ChainSearchResult search;
  search.found=out.found;
  search.node=out.node;
  search.scratch=out.scratch;
  write_helper_residue(machine, frame+8, bridge.entry_psr()|0x8000,
                       type, caller, frame,
                       0x7017EDA1u, (type>0) ? bridge.entry_carry() : 0, search);

  final_wsp = out.allocated ? wsp0+8 : wsp0;   // the normative wsp effect (H3)
  debug::Capture::native_footprint(machine);
  return bridge.native_return_ss(final_wsp);
}

uint32_t o_revert(hw::Machine& machine) {
  hw::RTBridge bridge(machine, hw::RTBridge::SS);
  int32_t type, raw_key2, caller, wsp0, frame;

  hw::RTStubs::log_call(machine, "O.REVERT", "(native)");
  type=bridge.entry_ac(0);
  raw_key2=bridge.entry_ac(1);
  caller=machine.wfp;
  wsp0=bridge.entry_wsp();
  frame=wsp0+12;

  bridge.emulate_frame_ss();                 // WSSVR image (pushed psr = entry psr)
  // Innermost-establisher gate + search + deactivate-in-place via the
  // api. The deactivation write ([node+2] = 0) now precedes the helper
  // residue below instead of following it (emulated order); the two are
  // address-disjoint (node cells vs [frame+2..frame+14]), so net memory
  // is identical.
  rt::RevertOutcome out;
  rt::error_handler().revert_node(machine, caller, type, raw_key2, out);
  if(out.gate_passed) {
    rt::ChainSearchResult search;
    search.found=out.found;
    search.node=out.node;
    search.scratch=out.scratch;
    // Helper residue: called with wsp = frame (WSSVR 0x0000); its
    // pushed psr carries ovk=0 (WSSVR clears ovk AFTER its pushes, in
    // O.REVERT's OWN save; the helper's pushed psr reflects the machine
    // at the XJSR: ovk=0).
    write_helper_residue(machine, frame, bridge.entry_psr()&~0x8000,
                         type, caller, frame,
                         0x7017EDD6u, (type>0) ? bridge.entry_carry() : 0, search);
  }

  debug::Capture::native_footprint(machine);
  return bridge.native_return_ss(wsp0);
}

} // namespace emu_rt
