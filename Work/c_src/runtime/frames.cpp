// src/runtime/frames.cpp — Project 3: I.PROLOG / I.EPILOG / I.GOTO
//
// Instruction-by-instruction ports (METHOD.md §1: never from intent).
// Derivation with the frame layout and per-instruction commentary:
// docs/Project3/DERIVATION.md. None of the three bodies contains a
// WSAVS/WSSVS, so there is no frame image to emulate; the footprint is
// the explicit stores below plus the machine-register end state, and
// each routine ends with RTBridge::native_transfer (transfer pairing,
// docs/SharedProtocol.md frozen interface 2).
//
// Carry/overflow come from EagleInstruction::add/sub themselves (thin
// forwards below), per METHOD.md §5: verify flag side effects from the
// emulator source, never from intent.
// P24 CORRECTION (Aug 2026, wide-carry fix): this header used to say
// "WSUB 0,0 clears carry; WSBI's borrow SETS carry when the count is 0
// (I.PROLOG exits c=1 for count 0, c=0 for count 1)" — those were the
// OLD (>>31) emulator's values. Under the manual-correct ALU carry:
// WSUB 0,0 SETS carry (x-x, no borrow); WSBI 1,1 on count 0 borrows
// (0-1) so c=0, on count 1 no borrow so c=1 — I.PROLOG exits c=0 for
// count 0, c=1 for count 1. The forwards make these values automatic.
//
// Project 8 Stage A: handler-STATE reads/writes (frame slots, chain
// head, snapshot) moved behind rt::error_handler_api; this file keeps
// the COMMON halves — argument decode, register/flag staging (the emu
// op replicas below), the normative wsp reservations (H3), stack
// surgery, and transfer pairing. mv_error_handler.cpp holds the
// verbatim-lifted state halves.
#include "frames.hpp"
#include "error_handler.hpp"
#include "../hw/Machine.hpp"
#include "../hw/Memory.hpp"
#include "../hw/EagleInstruction.hpp"
#include "../hw/RTBridge.hpp"
#include "../hw/RTStubs.hpp"
#include "../hw/Lockstep.hpp"
#include "../debug/Capture.hpp"
#include "../debug/CallStack.hpp"
#include <stdexcept>
#include <cstdio>

namespace {

// P24 CORRECTION (Aug 2026): these were verbatim REPLICAS of
// EagleInstruction::add/sub, which froze the >>31 wide-carry bug into
// native code (docs/Project24/CarryCensus.md finding F2). add/sub are
// static and public (IRExec calls them directly, P23 ruling), so the
// replicas are now thin forwards — single source of truth; the helper
// fix covers this file with no local formula to drift.
int32_t emu_add(hw::Machine& machine, int64_t src, int64_t dst) {
  return hw::EagleInstruction::add(machine, src, dst);
}
int32_t emu_sub(hw::Machine& machine, int64_t src, int64_t dst) {
  return hw::EagleInstruction::sub(machine, src, dst);
}

// Segment-corrected word address, as every X-format body access does
// (Machine::eagle_x_resolve_indirect tail: copy_segment against the
// instruction pc). All of our addresses live in segment 7 alongside the
// code, so this is the identity in practice; kept for exactness.
uint32_t seg(hw::Machine& machine, int32_t addr) {
  return hw::Machine::copy_segment(static_cast<uint32_t>(machine.pc),
                                   static_cast<uint32_t>(addr));
}

int32_t rd_wide(hw::Machine& machine, int32_t addr) {
  return static_cast<int32_t>(machine.memory->read_wide(seg(machine, addr)));
}
int32_t rd_word_sext(hw::Machine& machine, int32_t addr) {   // XNLDA
  uint32_t w = machine.memory->read_word(seg(machine, addr));
  return static_cast<int32_t>(w << 16) >> 16;
}
void wr_wide(hw::Machine& machine, int32_t addr, int32_t value) {
  machine.memory->write_wide(seg(machine, addr), value);
}

// Machine::wide_push replica without the upper-limit fault (the three
// bodies push at most 3 wides above a caller wsp the emulated path
// already survived with; a genuine overflow would have faulted the
// master identically before any translation existed).
void push_wide(hw::Machine& machine, int32_t value) {
  machine.wsp = machine.wsp + 2;
  machine.memory->write_wide(seg(machine, machine.wsp), value);
}

// WRTN replica (EagleStack.cpp:140-154) against the CURRENT wfp:
// restores caller registers and psr from the frame image, pops the
// argument words per the frame word's low half, pops the shadow
// call-stack entry (CallStack::call_return — including its benign
// mismatch notice when `ret` is a patched value, exactly as the
// emulated WRTN produces), and returns the resume pc.
uint32_t wrtn(hw::Machine& machine) {
  int32_t value, frame_word;

  int32_t pre_wfp = machine.wfp;   // M4a: area frame? fixup after the stock sequence
  machine.wsp = machine.wfp;
  value       = rd_wide(machine, machine.wsp); machine.wsp -= 2;
  machine.wfp = rd_wide(machine, machine.wsp); machine.wsp -= 2;
  machine.ac[2] = rd_wide(machine, machine.wsp); machine.wsp -= 2;
  machine.ac[1] = rd_wide(machine, machine.wsp); machine.wsp -= 2;
  machine.ac[0] = rd_wide(machine, machine.wsp); machine.wsp -= 2;
  frame_word  = rd_wide(machine, machine.wsp); machine.wsp -= 2;
  machine.ac[3] = machine.wfp;
  machine.set_psr(static_cast<int32_t>(static_cast<uint32_t>(frame_word) >> 16));
  frame_word = frame_word & 0x7FFF;
  machine.wsp = machine.wsp - 2 * frame_word;
  machine.c = static_cast<int32_t>(static_cast<uint32_t>(value) >> 31);
  machine.area_wrtn_fixup(pre_wfp);
  machine.call_stack->call_return(value & 0x7FFFFFFF);
  return static_cast<uint32_t>(value & 0x7FFFFFFF);
}

uint32_t fall_back(hw::Machine& machine, const char* name, const char* reason) {
  hw::RTStubs::log_call(machine, name, reason);
  machine.rt_pending_return = static_cast<uint32_t>(machine.ac[3]);
  return hw::RTStubs::entry_address(name);
}

} // namespace

namespace emu_rt {

// I.PROLOG (0x7017E733, 29 words) — build the condition frame inside
// the CALLER's WSAVS frame and resume at the continuation, entry-ac3+4
// (= LJSR pc + 7). Inline data after the LJSR: [ac3+0] wide -> [wfp+4],
// [ac3+2] narrow -> [wfp+6], [ac3+3] narrow = display count, code
// resumes at [ac3+4]. All 18 game sites pass 0/0/{0|1}; the display
// loop body runs count-1 times, i.e. never in Quest — but the loop's
// register/carry effects differ between count 0 and 1 and are
// replicated exactly.
uint32_t i_prolog(hw::Machine& machine) {
  int32_t entry_ac1, entry_ac3, entry_wsp, wfp0, inline_wide, inline_narrow;
  int32_t count, continuation;

  hw::RTStubs::log_call(machine, "I.PROLOG", "(native)");
  entry_ac1 = machine.ac[1];
  entry_ac3 = machine.ac[3];
  entry_wsp = machine.wsp;
  wfp0 = machine.wfp;

  // Register/flag staging, instruction for instruction; the stores the
  // emulated ops perform live in the api's establish() (mv lays them
  // bit-faithfully; a conforming implementation re-hosts them —
  // Registers E9/E10). wsp arithmetic here is the normative +4
  // reservation (H3); push CONTENTS are the api's.
  machine.ac[0] = emu_sub(machine, machine.ac[0], machine.ac[0]);  // WSUB 0,0: ac0=0, c=1 (P24: was c=0 pre-fix)
  machine.wsp += 4;                                     // WPSH 0,1 (head slot + entry ac1)
  machine.ac[2] = wfp0;                                 // LDAFP 2
  machine.ac[0] = machine.wsp;                          // LDASP 0 (the [wfp+2] snapshot value)
  machine.ac[0] = emu_sub(machine, 2, machine.ac[0]);   // WSBI 2,0 (the head-slot pointer)
  inline_wide = rd_wide(machine, entry_ac3 + 0x0);      // XWLDA 0,[ac3+0]
  machine.ac[0] = inline_wide;
  inline_narrow = rd_word_sext(machine, entry_ac3 + 0x2);  // XNLDA 0,[ac3+2]
  machine.ac[0] = inline_narrow;
  count = rd_word_sext(machine, entry_ac3 + 0x3);       // XNLDA 1,[ac3+3]
  machine.ac[1] = count;
  machine.wsp += 2;                                     // XPEF [ac3+4]: continuation EA push
  machine.ac[3] = machine.wsb;                          // LDASB 3

  rt::EstablishIn in;
  rt::EstablishOutcome out;
  in.frame = wfp0;
  in.entry_wsp = entry_wsp;
  in.entry_ac1 = entry_ac1;
  in.entry_ac3 = entry_ac3;
  in.slot4 = inline_wide;
  in.slot6 = inline_narrow;
  in.count = count;
  rt::error_handler().establish(machine, in, out);      // chain push + frame slots + display copies

  machine.ac[0] = out.old_head;                         // XWLDA 0,[wsb-0x40] (pre-push head)
  // Display-loop register/flag effects (memory copies are the api's):
  // WSBI 1,1 decrements with borrow; the body advances ac2 by 2 per
  // iteration. Exit c/ovr come from the final decrement, exactly as
  // the emulated loop leaves them (P24 fixed values: c=0 for count 0
  // — the 0-1 decrement borrows — and c=1 for count 1; pre-fix these
  // were reversed).
  machine.ac[2] = wfp0 + 0xC;                           // XLEF 2,[wfp+0xC]
  while(true) {
    machine.ac[1] = emu_sub(machine, 1, machine.ac[1]); // WSBI 1,1
    if(machine.ac[1] <= 0)                              // WSGT 1,1
      break;
    machine.ac[2] = emu_add(machine, 2, machine.ac[2]); // WADI 2,2
  }
  machine.ac[3] = machine.wfp;                          // LDAFP 3
  // WPOPJ: pops the continuation EA the XPEF pushed. The popped VALUE
  // is entry_ac3+4 by construction; using it directly (instead of
  // reading the stack cell back) keeps the wrapper independent of
  // whether the implementation wrote the push contents (Register E9).
  continuation = entry_ac3 + 0x4;
  machine.wsp -= 2;
  debug::Capture::native_footprint(machine);
  return hw::RTBridge::native_transfer(machine,
           static_cast<uint32_t>(continuation));
}

// I.EPILOG (0x7017E77D, 7 words) — unlink the condition frame
// ([wsb-0x40] = [wfp+8]) and then perform the CALLER's OWN return: the
// WRTN executes against the game routine's frame, so control resumes
// at the game routine's return address with its frame and arguments
// popped. "Never returns conventionally" — the LJSR return address in
// ac3 is never used.
uint32_t i_epilog(hw::Machine& machine) {
  uint32_t resume;

  hw::RTStubs::log_call(machine, "I.EPILOG", "(native)");
  machine.ac[3] = machine.wfp;                          // LDAFP 3
  // Chain pop (XWLDA 0,[wfp+8]; XWSTA 0,[wsb-0x40]) via the api; the
  // emulated body's intermediate ac0/ac2 values are erased by the WRTN
  // below (final registers come entirely from the caller's frame
  // image), so no register staging is owed here.
  rt::error_handler().disestablish(machine, machine.wfp);
  resume = wrtn(machine);                               // WRTN (the caller's)
  debug::Capture::native_footprint(machine);
  return hw::RTBridge::native_transfer(machine, resume);
}

// I.GOTO (0x7017EC7C, 80 words) — the non-local unwind. Entry: ac0 =
// target frame pointer, ac2 = label pc, ac3 = LJSR return (dead unless
// the error path signals). Three shapes:
//
//  * local (ac0 == wfp): XJMP [ac2+0] — jump to the label, no state
//    changes beyond ac1=entry ac3, ac3=wfp.
//  * unwind (live shape, 26 game sites): walk the saved-wfp chain from
//    wfp until the frame BELOW the cursor is the target, advancing the
//    condition-chain head [wsb-0x40] past every frame walked; patch the
//    cursor frame's ret slot to the landing stub 0x7017EC9D and its
//    saved-ac2 slot to the label pc; wfp = cursor; WRTN (pops the
//    cursor frame, restoring the TARGET frame as wfp/ac3 and the label
//    into ac2, c=0 from the patched slot's clear bit 31); the stub then
//    restores wsp from [target+2] (I.PROLOG's snapshot) and jumps
//    XJMP [ac2+0] to the label.
//  * error/cross-segment (0x7017ECA2/ECBD/ECC1: corrupt chain or a
//    target off this stack, ending in O.SERROR or the [0x70000124]
//    dispatch): NEVER observed live; the walk is read-only so the
//    shape is detected before any side effect and the wrapper falls
//    back to emulation (METHOD.md §7 — the master emulates the same
//    path; symmetric).
uint32_t i_goto(hw::Machine& machine) {
  int32_t target, label, cursor, below;
  uint32_t resume;
  int guard;

  target = machine.ac[0];
  label = machine.ac[2];

  // Project 8 H7: the deliberate bad-token one-shot (QUEST_BAD_TOKEN;
  // native/check only — in mv mode the corruption would desynchronize
  // the bit-faithful attic from the master instead of exercising the
  // abort). 0x100 is a positive non-frame value: the pre-walk descends
  // the real chain to its bottom and hits the non-positive-link shape.
  if(hw::RTStubs::bad_token_armed &&
     rt::handler_mode != rt::HandlerMode::MV && target != machine.wfp) {
    hw::RTStubs::bad_token_armed = false;
    hw::RTStubs::log_call(machine, "I.GOTO", "(BAD-TOKEN one-shot: corrupting target)");
    machine.ac[0] = 0x100;
    target = 0x100;
  }

  // Local goto: no walk, no side effects before the branch.
  if(target == machine.wfp) {
    hw::RTStubs::log_call(machine, "I.GOTO", "(native)");
    machine.ac[1] = machine.ac[3];                      // WMOV 3,1
    machine.ac[3] = machine.wfp;                        // LDAFP 3
    debug::Capture::native_footprint(machine);
    return hw::RTBridge::native_transfer(machine,
             seg(machine, label));                      // XJMP [ac2+0]
  }

  // Pre-walk (reads only): classify the shape before the first store.
  // In mv mode the bad shapes keep the bit-faithful fallback (the
  // attic emulates the defensive branches symmetrically with the
  // master). In native/check they are ABORT-INTENDED (ruling d /
  // contract THIRD ADDENDUM item 2 + §3.3 shape 3a): abort_world
  // (save=false) with the token and pc named — there is no coherent
  // state to fall back to emulation with.
  below = machine.wfp;
  guard = 0;
  while(true) {
    cursor = below;
    below = rd_wide(machine, cursor - 0x2);             // saved wfp
    // Ruling A (Project 12): the descent test compares in MASTER stack
    // coordinates — frame_precedes (Mapper.md §1.3, Q1-a ruling) maps
    // area frames back onto the master's stack, so an area frame linked
    // under a real-stack frame is descending exactly when the master's
    // chain is. Operands only; logic untouched.
    if(!machine.frame_precedes(below, cursor) || below <= 0) {   // 0x7017ECA2 / ECBD shapes
      if(rt::handler_mode == rt::HandlerMode::MV)
        return fall_back(machine, "I.GOTO", "(native-fallback: non-descending or non-positive frame link)");
      char buf[192];
      snprintf(buf, sizeof(buf),
               "L2 ABORT-INTENDED: I.GOTO bad-chain shape (token ac0=%08X, link %08X under cursor %08X, entry pc=%08X)",
               static_cast<uint32_t>(target), static_cast<uint32_t>(below),
               static_cast<uint32_t>(cursor), static_cast<uint32_t>(machine.pc));
      hw::Lockstep::abort_world(buf, &machine, /*save=*/false);
      throw std::runtime_error(buf);
    }
    if(below == target)
      break;
    if(++guard > 4096) {
      if(rt::handler_mode == rt::HandlerMode::MV)
        return fall_back(machine, "I.GOTO", "(native-fallback: walk guard)");
      char buf[160];
      snprintf(buf, sizeof(buf),
               "L2 ABORT-INTENDED: I.GOTO walk guard — token ac0=%08X not found in 4096 descending frames (entry pc=%08X)",
               static_cast<uint32_t>(target), static_cast<uint32_t>(machine.pc));
      hw::Lockstep::abort_world(buf, &machine, /*save=*/false);
      throw std::runtime_error(buf);
    }
  }

  hw::RTStubs::log_call(machine, "I.GOTO", "(native)");
  machine.ac[1] = machine.ac[3];                        // WMOV 3,1
  machine.ac[3] = machine.wfp;                          // LDAFP 3
  push_wide(machine, machine.ac[1]);                    // WPSH 1,2 (residue above the cut;
  push_wide(machine, machine.ac[2]);                    //   stack residue stays common — E8)
  // The 0x7017EC84 head-advance walk and the [wsb-0x40] write are the
  // api's chain-cut bookkeeping (mv replays the walk on locals — the
  // emulated loop's register scratch is erased by the WRTN below).
  // The MV-stack cut itself — the cursor patches, the WRTN through the
  // cut frame, and the snapshot restore — is M3b-unchanged stack
  // surgery and stays here (NativeDesign §1, I.GOTO row). The pre-walk
  // above already found the cursor.
  rt::CutOutcome cut;
  rt::error_handler().cut(machine, target, machine.wfp, cut);
  machine.ac[0] = static_cast<int32_t>(seg(machine, 0x7017EC9D));  // XLEF 0,[pc+8]
  wr_wide(machine, cursor + 0x0, machine.ac[0]);        // patch cursor ret -> landing stub
  machine.ac[0] = rd_wide(machine, machine.wsp);        // WPOP 0,0: the pushed label pc
  machine.wsp -= 2;
  wr_wide(machine, cursor - 0x4, machine.ac[0]);        // patch cursor saved-ac2 -> label
  machine.wfp = cursor;                                 // STAFP 2
  resume = wrtn(machine);                               // WRTN -> lands on the stub;
  (void)resume;                                         //   (resume == 0x7017EC9D by construction)
  // Landing stub 0x7017EC9D: XWLDA 0,[ac3+2]; STASP 0; fall into
  // 0x7017ECA0 XJMP [ac2+0]. The loaded value is the establishment
  // snapshot — from the outcome, not a raw [target+2] read, so the
  // wrapper works whether the snapshot lives in the frame slot (mv) or
  // in the re-hosted record (Register E9).
  machine.ac[0] = cut.wsp_restore;
  machine.wsp = machine.ac[0];                            // STASP 0
  // M4a: every redirected frame above the target is gone with the cut —
  // drop their live records (the target's own record, if any, stays).
  machine.area_unwind_to(target);
  debug::Capture::native_footprint(machine);
  return hw::RTBridge::native_transfer(machine,
           seg(machine, machine.ac[2]));                  // XJMP [ac2+0] -> label
}

} // namespace emu_rt
