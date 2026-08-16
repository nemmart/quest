// src/runtime/mv_error_handler.cpp — Project 8 Stage A.
//
// Every body below is a VERBATIM LIFT of the state half of a
// pre-Project-8 wrapper (frames.cpp / o_on.cpp / o_signal.cpp), with
// the per-origin addressing convention preserved: frames.cpp-derived
// writes go through the segment-corrected helpers (seg/rd/wr below,
// identical formulas), o_on/o_signal-derived writes use the raw casts
// those files used. Cross-reference comments name the source lines'
// original disassembly anchors so this file still audits against the
// listings.
#include "mv_error_handler.hpp"
#include "o_on.hpp"          // rt::chain_search (the EE7A helper)
#include "o_signal.hpp"      // rt::select_frames, rt::signal_walker
#include "../hw/Machine.hpp"
#include "../hw/Memory.hpp"

namespace {

// frames.cpp addressing helpers, copied exactly (segment correction
// against the instruction pc — identity in practice, kept for
// exactness; METHOD §5).
uint32_t seg(hw::Machine& machine, int32_t addr) {
  return hw::Machine::copy_segment(static_cast<uint32_t>(machine.pc),
                                   static_cast<uint32_t>(addr));
}
int32_t rd_wide(hw::Machine& machine, int32_t addr) {
  return static_cast<int32_t>(machine.memory->read_wide(seg(machine, addr)));
}
void wr_wide(hw::Machine& machine, int32_t addr, int32_t value) {
  machine.memory->write_wide(seg(machine, addr), value);
}

} // namespace

namespace rt {

// ---------------------------------------------------------------------
// I.PROLOG — the memory half of frames.cpp i_prolog, in the emulated
// write order. The register/flag effects (WSUB/WSBI/loop arithmetic)
// are staged by the COMMON wrapper; only stores live here.
void mv_error_handler::establish(hw::Machine& machine, const EstablishIn& in,
                                 EstablishOutcome& out) {
  int32_t wfp0 = in.frame;

  wr_wide(machine, in.entry_wsp + 2, 0);              // WPSH 0,1: the zeroed head slot
  wr_wide(machine, in.entry_wsp + 4, in.entry_ac1);   //   and entry ac1 above it
  wr_wide(machine, wfp0 + 0x2, in.entry_wsp + 4);     // [wfp+2] = wsp snapshot (GOTO's STASP source)
  wr_wide(machine, wfp0 + 0xA, in.entry_wsp + 2);     // [wfp+0xA] = &head slot (O.ON follows it)
  wr_wide(machine, wfp0 + 0x4, in.slot4);             // [wfp+4] = inline wide
  wr_wide(machine, wfp0 + 0x6, in.slot6);             // [wfp+6] = inline narrow
  wr_wide(machine, in.entry_wsp + 6, in.entry_ac3 + 0x4);   // XPEF [ac3+4]: continuation EA (residue above final wsp)
  out.old_head = rd_wide(machine, machine.wsb - 0x40);      // XWLDA 0,[wsb-0x40]
  wr_wide(machine, machine.wsb - 0x40, wfp0);         // head = this frame
  wr_wide(machine, wfp0 + 0x8, out.old_head);         // [wfp+8] = enclosing link
  // Display copy (common tail 0x7017E76E): count-1 iterations walking
  // the caller's static-link chain from [wfp-6]. Never iterates in
  // Quest (all sites pass 0 or 1); the register/carry effects of the
  // loop are the wrapper's; the copies are ours.
  {
    int32_t walk = rd_wide(machine, wfp0 - 0x6);      // XWLDA 3,[wfp-6]
    int32_t dst = wfp0 + 0xC;                         // XLEF 2,[wfp+0xC]
    int32_t remaining = in.count;
    while(--remaining > 0) {                          // WSBI 1,1 / WSGT 1,1 shape
      walk = rd_wide(machine, walk - 0x6);            // XWLDA 3,[ac3-6]
      wr_wide(machine, dst, walk);                    // XWSTA 3,[ac2]
      dst += 2;                                       // WADI 2,2
    }
  }
}

// ---------------------------------------------------------------------
// I.EPILOG — chain pop: [wsb-0x40] = [wfp+8], unconditionally (the
// bit-faithful tolerant behavior; the ABORT-INTENDED mismatch check —
// THIRD ADDENDUM item 1 — belongs to the conforming implementation,
// Stage B, not to this attic).
void mv_error_handler::disestablish(hw::Machine& machine, int32_t caller_wfp) {
  int32_t old_head = rd_wide(machine, caller_wfp + 0x8);   // XWLDA 0,[wfp+8]
  wr_wide(machine, machine.wsb - 0x40, old_head);          // XWSTA 0,[wsb-0x40]
}

// ---------------------------------------------------------------------
// I.GOTO unwind — the 0x7017EC84 head-advance walk and the head write,
// on locals (the register scratch the emulated loop uses is erased by
// the WRTN the common wrapper performs; final machine state is
// identical). The common wrapper's pre-walk has already proven the
// chain descends to the target, so the loop terminates.
void mv_error_handler::cut(hw::Machine& machine, int32_t target_frame,
                           int32_t entry_wfp, CutOutcome& out) {
  int32_t head = rd_wide(machine, machine.wsb - 0x40);     // XWLDA 1,[wsb-0x40]
  int32_t cur = entry_wfp;
  while(true) {                                            // 0x7017EC84 loop
    if(cur == head)                                        // WSEQ 3,1
      head = rd_wide(machine, cur + 0x8);                  // head past this frame
    int32_t prev = cur;                                    // WMOV 3,2 (cursor)
    cur = rd_wide(machine, prev - 0x2);                    // XWLDA 3,[cursor-2]
    if(cur == target_frame)                                // WSEQ 3,0
      break;
  }
  wr_wide(machine, machine.wsb - 0x40, head);              // head after the unwind
  out.wsp_restore = rd_wide(machine, target_frame + 0x2);  // [target+2]: I.PROLOG's snapshot
}

// ---------------------------------------------------------------------
// O.ON — search + reuse/allocate, lifted from o_on.cpp. ORDER CONTRACT
// with the common wrapper: the wrapper lays the WSSVS image BEFORE
// calling this (the allocate path's relocated image and node overwrite
// parts of that image, exactly as the emulated relocation overwrites
// the real pushes), and lays the helper residue AFTER (address-disjoint
// from every write below; net memory identical to the emulated order).
void mv_error_handler::register_node(hw::Machine& machine, const RegisterIn& in,
                                     RegisterOutcome& out) {
  hw::Memory& memory = *machine.memory;
  rt::ChainSearchResult search;
  int32_t frame = in.entry_wsp + 12;             // O.ON's would-be WSSVS frame pointer
  int32_t key2 = (in.type > 0) ? in.raw_key2 : 0;   // helper preamble

  rt::chain_search(memory, in.caller_frame, in.type, key2, search);
  out.found = search.found;
  out.node = search.node;
  out.scratch = search.scratch;

  if(search.node != 0) {
    // Reuse path (found node, or the not-found backstop).
    int32_t node = search.node;
    memory.write_wide(static_cast<uint32_t>(node) + 0x2, in.type);
    memory.write_wide(static_cast<uint32_t>(node) + 0x4, (in.type <= 0) ? 0 : in.raw_key2);
    memory.write_wide(static_cast<uint32_t>(node) + 0x6, in.handler);
    out.allocated = false;
  }
  else {
    // Allocate path: the frame-extension trick (o_on.cpp commentary).
    int32_t node = frame - 10;
    memory.write_wide(static_cast<uint32_t>(frame) - 2, in.entry_psr << 16);
    memory.write_wide(static_cast<uint32_t>(frame) + 0, in.type);
    memory.write_wide(static_cast<uint32_t>(frame) + 2, in.raw_key2);
    memory.write_wide(static_cast<uint32_t>(frame) + 4, in.handler);
    memory.write_wide(static_cast<uint32_t>(frame) + 6, in.caller_frame);
    memory.write_wide(static_cast<uint32_t>(frame) + 8,
      static_cast<int32_t>(static_cast<uint32_t>(in.entry_ac3) |
                           (static_cast<uint32_t>(in.entry_carry) << 31)));   // relocated ret|c
    memory.write_wide(static_cast<uint32_t>(node) + 0x2, in.type);
    memory.write_wide(static_cast<uint32_t>(node) + 0x4, (in.type <= 0) ? 0 : in.raw_key2);
    memory.write_wide(static_cast<uint32_t>(node) + 0x6, in.handler);
    memory.write_wide(static_cast<uint32_t>(in.caller_frame) + 0x2, frame - 4);   // bookkeeping pointer
    int32_t head_slot = memory.read_wide(static_cast<uint32_t>(in.caller_frame) + 0xA);
    int32_t old_head = memory.read_wide(static_cast<uint32_t>(head_slot));
    memory.write_wide(static_cast<uint32_t>(node) + 0x0, old_head);
    memory.write_wide(static_cast<uint32_t>(head_slot), node);
    out.allocated = true;
  }
}

// ---------------------------------------------------------------------
// O.REVERT — gate on [wsb-0x40] == caller, search, deactivate in place.
void mv_error_handler::revert_node(hw::Machine& machine, int32_t caller_wfp,
                                   int32_t type, int32_t raw_key2,
                                   RevertOutcome& out) {
  hw::Memory& memory = *machine.memory;
  rt::ChainSearchResult search;

  out.gate_passed = false;
  out.found = false;
  out.node = 0;
  out.scratch = 0;
  int32_t current = memory.read_wide(static_cast<uint32_t>(machine.wsb) - 0x40);
  if(current != caller_wfp)
    return;
  out.gate_passed = true;
  int32_t key2 = (type > 0) ? raw_key2 : 0;
  rt::chain_search(memory, current, type, key2, search);
  out.found = search.found;
  out.node = search.node;
  out.scratch = search.scratch;
  if(search.found)
    memory.write_wide(static_cast<uint32_t>(search.node) + 0x2, 0);   // deactivate in place
}

// ---------------------------------------------------------------------
// The select loop / has-handler predicate — rt::select_frames verbatim
// (o_signal.cpp keeps the walk; it is mv-internal since Project 8).
void mv_error_handler::select(hw::Machine& machine, int32_t type,
                              int32_t raw_key2, SelectOutcome& out) {
  rt::SelectResult sel;
  rt::select_frames(machine, type, raw_key2, sel);
  out.found = sel.found;
  out.frame = sel.frame;
  out.handler = sel.handler;
  out.any_search = sel.any_search;
  out.last_frame = sel.last_frame;
  out.last_node = sel.last_node;
  out.last_scratch = sel.last_scratch;
  out.last_found = sel.last_found;
}

bool mv_error_handler::has_handler(hw::Machine& machine) {
  // The ?LIB_ERROR prediction: exactly select_frames(-1, 0) (the
  // caller has already checked the walker gate).
  rt::SelectResult sel;
  rt::select_frames(machine, -1, 0, sel);
  return sel.found;
}

// ---------------------------------------------------------------------
// O.SET semantics — the live walker plus the five wsb-band stores, in
// the emulated store order (walker outputs EF00/EF02 first, then the
// record EE5B..EE5F). Raw-cast addressing per the o_signal.cpp origin.
void mv_error_handler::record_raise(hw::Machine& machine, const SigRecord& rec) {
  hw::Memory& mem = *machine.memory;
  uint32_t wsb = static_cast<uint32_t>(machine.wsb);
  rt::WalkerResult walk;
  rt::signal_walker(machine, walk);                 // pure reads
  mem.write_wide(wsb - 0x36, walk.out1);
  mem.write_wide(wsb - 0x38, walk.out2);
  mem.write_wide(wsb - 0x3E, rec.type);
  mem.write_wide(wsb - 0x3C, rec.key2);
  mem.write_wide(wsb - 0x3A, rec.code);
}

SigRecord mv_error_handler::sig_record(hw::Machine& machine) {
  hw::Memory& mem = *machine.memory;
  uint32_t area = static_cast<uint32_t>(machine.wsb) - 0x40;
  SigRecord rec;
  rec.type = static_cast<int32_t>(mem.read_wide(area + 0x2));
  rec.key2 = static_cast<int32_t>(mem.read_wide(area + 0x4));
  rec.code = static_cast<int32_t>(mem.read_wide(area + 0x6));
  return rec;
}

void mv_error_handler::set_resume_flag(hw::Machine& machine, int32_t flag) {
  machine.memory->write_wide(static_cast<uint32_t>(machine.wsb) - 0x2A, flag);   // EDF9
}

int32_t mv_error_handler::resume_flag(hw::Machine& machine) {
  return static_cast<int32_t>(
    machine.memory->read_wide(static_cast<uint32_t>(machine.wsb) - 0x2A));
}

} // namespace rt
