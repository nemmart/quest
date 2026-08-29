// src/runtime/o_signal.cpp
//
// Derivation, residue maps, and instruction-semantics evidence:
// docs/Project1/DERIVATION.md. Every memory write below is a line item
// in sec. 5 of that doc; the writes are laid in EMULATED EXECUTION
// ORDER (later writes overwrite earlier ones at [E+12] and [E+14..29]
// exactly as the emulated pushes would) so this file can be audited
// against the disassembly top to bottom.
#include "o_signal.hpp"
#include "o_on.hpp"          // rt::chain_search — the EE7A helper, already validated
#include "def_on.hpp"        // rt::def_on_would_run_native (Project 5 lift)
#include "error_handler.hpp" // Project 8: handler-state seam (select / record / flag)
#include "../hw/Machine.hpp"
#include "../hw/Memory.hpp"
#include "../hw/RTBridge.hpp"
#include "../hw/RTStubs.hpp"
#include "../hw/NativeRegistry.hpp"
#include "../os/OSProcess.hpp"
#include "../debug/Capture.hpp"
#include <stdexcept>
#include <cstdio>

namespace {

constexpr uint32_t GATE_WIDE_ADDR   = 0x7017EEA0u;  // WLDAI immediate in the EE9D walker
constexpr uint32_t DEF_ON_ENTRY     = 0x7017EF05u;  // exhaustion target (terminal frontier)
constexpr uint32_t OSET_XJSR_RET    = 0x7017EE5Au;  // walker return (XJSR at EE58)
constexpr uint32_t OSET_CALL_RET    = 0x7017EE3Bu;  // O.SET return (XCALL at EE38)
constexpr uint32_t HELPER_XJSR_RET  = 0x7017EE6Au;  // helper return (XJSR at EE68)
constexpr uint32_t DISPATCH_RET     = 0x7017EE40u;  // handler return (XCALL at EE3D)
constexpr int32_t  FRAME_WALK_LIMIT = 1024;         // cycle guard (loud, METHOD sec. 8)

int32_t rd(hw::Machine& m, uint32_t addr) {         // read_wide returns uint32_t;
  return static_cast<int32_t>(m.memory->read_wide(addr));   // compares are signed (METHOD sec. 12)
}

uint32_t fallback(hw::Machine& machine, const char* entry_name, const char* reason) {
  hw::RTStubs::log_call(machine, entry_name, reason);
  machine.rt_pending_return = static_cast<uint32_t>(machine.ac[3]);
  return hw::RTStubs::entry_address(entry_name);
}

// Nested-in-fallback guard: on the clone, rt_pending_return != 0 at
// native dispatch means we are INSIDE a fallback span — the outer
// routine is being re-emulated and the master is absorbing its whole
// body (including this inner entry) into one run-to-return batch. If
// this inner call ran natively, the master would emulate instructions
// the clone skips, and the span's terminal pair (which compares
// instruction counts — hw/Lockstep.cpp compare_pair) would diverge.
// Emulate instead, WITHOUT re-arming rt_pending_return (re-arming
// would retarget the outer span's return). Found empirically: the
// no-handler signal falls back, and its emulated body XCALLs O.SET,
// which is also registered. (Masters and non-lockstep runs never
// dispatch natives, so this guard is clone-only by construction.)
bool nested_in_fallback(hw::Machine& machine, const char* entry_name) {
  if(machine.rt_pending_return == 0)
    return false;
  hw::RTStubs::log_call(machine, entry_name, "(native-skip: inside fallback span)");
  return true;
}

} // namespace

namespace rt {

bool walker_gate_open(hw::Machine& machine) {
  // WLDAI ac0,[EEA0]; WSGT 0,0 — skip (into the I?LINEID region) iff
  // the immediate is > 0. The emulator re-reads code space on every
  // execution, so the native reads it too rather than assuming 0.
  // Code-space state, NOT handler state: common to every -handler=
  // implementation (Project 8).
  return rd(machine, GATE_WIDE_ADDR) > 0;
}

// signal_walker / select_frames below are mv-internal since Project 8:
// no wrapper calls them directly (they walk real-memory chain state);
// mv_error_handler is their only caller. Kept exported here to avoid
// moving validated code — consolidation is a REPORT.md cleanup
// candidate.

void signal_walker(hw::Machine& machine, WalkerResult& out) {
  out.out1 = 0;                                     // EEFA/EEFB defaults (WSUB 2,2 / 1,1)
  out.out2 = 0;
  int32_t frame = rd(machine, static_cast<uint32_t>(machine.wsb) - 0x40);
  int32_t guard = 0;
  while (frame > 0) {                               // EEED WSGT 3,3
    if (++guard > FRAME_WALK_LIMIT)
      throw std::runtime_error("O.SET walker: frame chain exceeds 1024 (cycle?)");
    int32_t p = rd(machine, static_cast<uint32_t>(frame) + 0x4);   // EEEF
    if (p > 0) {                                    // EEF1 WSGT 2,2
      int32_t w = static_cast<int32_t>(machine.memory->read_word(static_cast<uint32_t>(p)));
      w = (w << 16) >> 16;                          // EEF3 XNLDA sign-extends
      if (w > 0) {                                  // EEF5 WSLE 1,1 (not taken)
        out.out1 = rd(machine, static_cast<uint32_t>(frame) + 0x6); // EEFD
        out.out2 = p;                               // ac2 still holds p
        return;
      }
    }
    frame = rd(machine, static_cast<uint32_t>(frame) + 0x8);       // EEF7
  }
}

void select_frames(hw::Machine& machine, int32_t type, int32_t raw_key2,
                   SelectResult& out) {
  // Helper preamble (per invocation): catch-all (type<=0) searches with
  // key2 = 0. ac1 is re-loaded from the TOS backup each iteration
  // (LDATS at EE6B), so the effective key2 is loop-invariant.
  int32_t key2 = (type > 0) ? raw_key2 : 0;
  rt::ChainSearchResult search;

  out.found = false;
  out.frame = 0;
  out.handler = static_cast<int32_t>(DEF_ON_ENTRY);
  out.any_search = false;
  out.last_frame = 0;
  out.last_node = 0;
  out.last_scratch = 0;
  out.last_found = false;

  int32_t frame = rd(machine, static_cast<uint32_t>(machine.wsb) - 0x40);   // EE64
  int32_t guard = 0;
  while (frame > 0) {                               // EE66 WSGT 2,2
    if (++guard > FRAME_WALK_LIMIT)
      throw std::runtime_error("O?SIGNAL select loop: frame chain exceeds 1024 (cycle?)");
    rt::chain_search(*machine.memory, frame, type, key2, search);   // EE68 XJSR EE7A
    out.any_search = true;
    out.last_frame = frame;
    out.last_node = search.node;
    out.last_scratch = search.scratch;
    out.last_found = search.found;
    if (search.found) {                             // ret+0 -> EE75
      out.found = true;
      out.frame = frame;                            // WXCH: ac1 = frame
      out.handler = rd(machine, static_cast<uint32_t>(search.node) + 0x6);  // EE77
      return;
    }
    frame = rd(machine, static_cast<uint32_t>(frame) + 0x8);        // EE6C
  }
  // exhausted: EE6F..EE74 — handler = DEF?ON, ac1 = 0 (already set)
}

} // namespace rt

namespace emu_rt {

namespace {

// The shared signal body from the EE38 XCALL through the EE3D dispatch,
// as executed by O?SIGNAL and every shorthand. Lays the complete
// footprint (DERIVATION.md sec. 5) and TRANSFERS to the handler or to
// DEF?ON. The handler-returned tail (EE40-EE55) stays emulated: the
// pushed return address is EE40, inside emulated code, and both
// engines run it symmetrically if a handler ever returns.
//
//   type / key2_raw / code : ac0 / ac1 / ac2 at the EE38 XCALL
//   c_x                    : carry at the EE38 XCALL (per entry:
//                            O?SIGNAL argc>3 keeps the entry carry,
//                            every other live shape has c_x = 0)
uint32_t signal_dispatch(hw::Machine& machine, hw::RTBridge& bridge,
                         const char* entry_name,
                         int32_t type, int32_t key2_raw, int32_t code,
                         int32_t c_x) {
  hw::Memory& mem = *machine.memory;
  rt::SelectOutcome sel;
  int32_t entry_wfp = machine.wfp;   // the raiser's frame — the H6 tail's
                                     // WRTN restore target (Stage B)

  rt::error_handler().select(machine, type, key2_raw, sel);   // pure reads

  // No handler: the dispatch target would be DEF?ON — a TERMINAL entry
  // inside the RT range. Terminal pairs compare instruction counts
  // (count exemption needs native_span on BOTH sides — hw/Lockstep.cpp),
  // so a native transfer there can never pair: the master emulates the
  // whole body while the clone skips it. Fall back to emulation instead
  // (decided on pure reads, before any store): both engines then emulate
  // identically, converge at DEF?ON with equal counts and both terminal
  // flags set, and the terminal machinery detaches the clone. This
  // corrects SharedProtocol.md's "simply native_transfer to DEF?ON"
  // guidance — see REPORT.md, with the divergence report as evidence.
  // No handler: the dispatch target is DEF?ON. Pre-lift this was an
  // RT-range TERMINAL, so the only pairable move was a whole fallback
  // (the corrected SharedProtocol composition rule). The Project 5
  // lift makes DEF?ON ordinary translated L2: when its translation is
  // registered, lay the identical footprint and dispatch it through
  // the registry (RT-internal call to a translated sibling) instead
  // of transferring; DEF?ON's own gates decide everything deeper
  // (resume, resignal, or a whole-cascade fallback to the NEW
  // terminal, ?FATAL). When it is NOT registered (the pre-lift
  // configuration, or a partial build), the old whole-fallback stays.
  bool def_on_translated =
    hw::RTStubs::active &&
    DEF_ON_ENTRY >= hw::RTStubs::start && DEF_ON_ENTRY < hw::RTStubs::stop &&
    hw::RTStubs::translated_bits[DEF_ON_ENTRY - hw::RTStubs::start] != 0;
  if(!sel.found &&
     !(def_on_translated &&
       (rt::handler_mode != rt::HandlerMode::MV ||
        rt::def_on_would_run_native(machine, type, (type > 0) ? key2_raw : 0))))
    // Pre-lift configuration (DEF?ON untranslated): fall back whole.
    // MV attic (Generation-2 relic, kept verbatim): DEF?ON's own gates
    // predict a FALLBACK toward the ?FATAL terminal, and under strict
    // terminal counts a native prefix would skew the compared counts,
    // so the whole chain must emulate (both engines symmetric).
    // Generation 3 (native/check modes): the terminal pair is a
    // crossing (docs/CheckerHistory.md) — dispatch translated DEF?ON
    // unconditionally; its own branches handle resume, resignal, and
    // the native ?FATAL descent.
    return fallback(machine, entry_name,
                    "(native-fallback: no handler — terminal-bound, emulating for terminal pairing)");

  int32_t E = bridge.entry_wsp();
  uint32_t Eu = static_cast<uint32_t>(E);
  int32_t F = E + 10;                               // own WSAVS frame pointer
  int32_t psr_body = bridge.entry_psr() | 0x8000;   // ovk=1 after WSAVS; ovr=0 since the LCALL

  // --- own WSAVS image: [E+2..E+11] (entry ac0/ac1/ac2, wfp, ret|c) ---
  bridge.emulate_frame();

  // --- EE38 XCALL O.SET: frame word (psr<<16)|0 at [E+12] ---
  mem.write_wide(Eu + 12, psr_body << 16);
  // --- O.SET's WSAVS image at [E+14..E+23] (frame F2 = E+22) ---
  mem.write_wide(Eu + 14, type);
  mem.write_wide(Eu + 16, key2_raw);
  mem.write_wide(Eu + 18, code);
  mem.write_wide(Eu + 20, F);
  mem.write_wide(Eu + 22, static_cast<int32_t>(OSET_CALL_RET | (static_cast<uint32_t>(c_x) << 31)));
  // --- walker's WSSVR image at [E+24..E+35]; locals [E+36..45] untouched ---
  mem.write_wide(Eu + 24, psr_body << 16);
  mem.write_wide(Eu + 26, type);
  mem.write_wide(Eu + 28, key2_raw);
  mem.write_wide(Eu + 30, code);
  mem.write_wide(Eu + 32, E + 22);
  mem.write_wide(Eu + 34, static_cast<int32_t>(OSET_XJSR_RET | (static_cast<uint32_t>(c_x) << 31)));
  // --- the O.SET record: walker outputs (EF00/EF02) + the record
  //     stores (EE5B..EE5F), at this exact block position, via the api
  //     (mv runs the live walker and lays all five state cells;
  //     conforming implementations store the triple — ruling b) ---
  rt::error_handler().record_raise(machine, rt::SigRecord{type, key2_raw, code});

  // --- EE3B XPSHJ: wide EE3D at [E+12] (overwrites the O.SET frame word) ---
  mem.write_wide(Eu + 12, 0x7017EE3D);
  // --- EE62 WPSH 1,1: key2 backup at [E+14] (ac1 restored by O.SET's WRTN) ---
  mem.write_wide(Eu + 14, key2_raw);
  // --- helper residue: only the LAST invocation survives at [E+16..E+29];
  //     none at all when the frame chain was empty ---
  int32_t c_h = (type > 0) ? c_x : 1;               // catch-all: helper preamble WSUB 1,1 at
                                                    // ee7B SETS carry (x-x, no borrow) under the
                                                    // P24 fix (pre-fix it cleared it)
  uint32_t ret_wide = HELPER_XJSR_RET | (static_cast<uint32_t>(c_h) << 31);
  mem.write_wide(Eu + 16, psr_body << 16);          // (found implies any_search)
  mem.write_wide(Eu + 18, type);
  mem.write_wide(Eu + 20, sel.last_node);           // saved-ac1 slot patched with the result
  mem.write_wide(Eu + 22, sel.last_frame);          // saved ac2 = the searched frame
  mem.write_wide(Eu + 24, F);                       // saved wfp
  mem.write_wide(Eu + 26, static_cast<int32_t>(ret_wide));   // found: ret NOT ISZTS-incremented
  mem.write_wide(Eu + 28, sel.last_scratch);        // abandoned scratch / STATS backstop slot
  // --- EE3D XCALL through ac2: frame word at [E+12] (overwrites the XPSHJ wide) ---
  mem.write_wide(Eu + 12, psr_body << 16);

  // --- machine state at the transfer (DERIVATION.md sec. 5 register list) ---
  machine.wsp = E + 12;
  machine.wfp = F;
  machine.ovk = 1;
  machine.ovr = 0;                                  // XCALL clears it (already 0 since the LCALL)
  machine.ac[0] = type;
  machine.ac[1] = sel.frame;                        // registering frame (the handler's unwind argument)
  machine.ac[2] = sel.handler;
  machine.ac[3] = static_cast<int32_t>(DISPATCH_RET);
  machine.c = c_h;

  debug::Capture::native_footprint(machine);
  if(!sel.found) {
    // Exhaustion dispatch to translated DEF?ON (Project 5 lift): the
    // staged state above is exactly the XCALL-shape DEF?ON entry
    // (ac2 = DEF_ON_ENTRY = sel.handler, ac3 = DISPATCH_RET, frame
    // word (psr<<16)|0 at [E+12]) — an RT-internal native call, not a
    // native_transfer (the target is in-range; SharedProtocol rule).
    uint32_t pc3 = machine.process->native_registry.lookup(DEF_ON_ENTRY)(machine);
    if(pc3 == DISPATCH_RET) {
      // DEF?ON RESUMED (native_return aimed at the EE40 handler-
      // returned tail).
      if(rt::handler_mode == rt::HandlerMode::MV) {
        // Attic path (pre-Stage-B behavior, bit-faithful): the tail
        // and the unwind back to the raiser are shared emulated code —
        // clear the inner break and arm rt_pending_return to THIS
        // entry's return, so the clone emulates to the raiser's
        // post-call pc, where the master's run-to-return also ends
        // (native_span pair; count exemption covers the prefix).
        machine.native_break = false;
        machine.rt_pending_return = bridge.entry_return();
        return pc3;
      }
      // H6 (Stage B): NATIVE handling of the tail after the
      // DISPATCH_RET pair — replacing the arm-whole-span-emulation
      // continuation. Required for native mode, where the tail's
      // emulated reads ([wsb-0x2A] at EE41, O?SIGNAL's frame image at
      // the EE44 WRTN) would hit cells the clone never writes
      // (Registers E8/E9/E10). The live shape through DEF?ON's resume
      // gate has the flag's sign bit set, so EE41/EE43 take the EE44
      // WRTN: O?SIGNAL's own return to the raiser — staged from the
      // bridge's saved entry state (image-free), with wfp restored to
      // the raiser's frame (the value the emulated WRTN would pop).
      // Pairing is the standard translated-L2 exit crossing: the
      // master's run-to-return ends at the raiser's post-call pc, the
      // clone's native_return breaks there — same rendezvous, no
      // checker change. (The genuine handler-WRTN arrival at EE40
      // keeps its own depth-0 rendezvous in run_steps, untouched.)
      int32_t flag = rt::error_handler().resume_flag(machine);
      if(flag == 0)
        // Unreachable through DEF?ON's gates (defq_on and the
        // would-run predicate both require the sign bit); reaching it
        // means the implementations disagree — fail loudly, never
        // guess at the cold escalation branches (METHOD §9).
        throw std::runtime_error(
          "O?SIGNAL H6 tail: DISPATCH_RET with resume flag 0 — unreachable through DEF?ON's resume gate");
      machine.wfp = entry_wfp;         // EE44 WRTN: caller frame restored
      machine.wsp = bridge.entry_wsp();   // native_return pops frame word + args from here
      return bridge.native_return();
    }
    return pc3;   // resignal transfer (game pc) or belt-and-braces fallback
  }
  return hw::RTBridge::native_transfer(machine, static_cast<uint32_t>(sel.handler));
}

// Shorthand shape: WSAVS; fixed ac0/ac2; ac1 zeroed (WSUB 1,1, which
// also SETS carry under the P24 fix) at EE37 before the shared body. c_x = 1 for every
// shorthand and for O.SERROR.
uint32_t shorthand(hw::Machine& machine, const char* name, int32_t type, int32_t code) {
  if (nested_in_fallback(machine, name))
    return hw::RTStubs::entry_address(name);
  hw::RTBridge bridge(machine);
  if (rt::walker_gate_open(machine))
    return fallback(machine, name, "(native-fallback: gate wide 0x7017EEA0 > 0 — I?LINEID mode enabled)");
  hw::RTStubs::log_call(machine, name, "(native)");
  return signal_dispatch(machine, bridge, name, type, /*key2_raw=*/0, code, /*c_x=*/1);
}

} // namespace

uint32_t o_qsignal(hw::Machine& machine) {
  if (nested_in_fallback(machine, "O?SIGNAL"))
    return hw::RTStubs::entry_address("O?SIGNAL");
  hw::RTBridge bridge(machine);
  if (rt::walker_gate_open(machine))
    return fallback(machine, "O?SIGNAL", "(native-fallback: gate wide 0x7017EEA0 > 0 — I?LINEID mode enabled)");
  hw::RTStubs::log_call(machine, "O?SIGNAL", "(native)");

  // Body (EDEF..EE01): flag = argc>3 ? *arg4 (narrow, sign-extended) : 0,
  // stored to [wsb-0x2A]; ac0/ac1/ac2 = *arg1/*arg2/*arg3 (wide). The
  // argc<=3 path's WSUB 0,0 SETS carry (P24 fix; pre-fix it cleared
  // it); the argc>3 path keeps the entry carry.
  int32_t flag, c_x;
  if (bridge.arg_count() > 3) {                     // EDF2 WSGTI 0,3
    flag = bridge.arg_word(4);                      // EDF5 XNLDA @[ac3-0x12]
    c_x = bridge.entry_carry();
  } else {
    flag = 0;                                       // EDF8 WSUB 0,0 (c=1 under the P24 fix)
    c_x = 1;
  }
  rt::error_handler().set_resume_flag(machine, flag);   // EDF9 [wsb-0x2A] (H4: only this
                                                        //   entry path ever writes it)

  int32_t type = bridge.arg_wide(1);                // EDFB
  int32_t key2 = bridge.arg_wide(2);                // EDFD
  int32_t code = bridge.arg_wide(3);                // EDFF
  return signal_dispatch(machine, bridge, "O?SIGNAL", type, key2, code, c_x);
}

uint32_t o_set(hw::Machine& machine) {
  if (nested_in_fallback(machine, "O.SET"))
    return hw::RTStubs::entry_address("O.SET");
  hw::RTBridge bridge(machine);
  if (rt::walker_gate_open(machine))
    return fallback(machine, "O.SET", "(native-fallback: gate wide 0x7017EEA0 > 0 — I?LINEID mode enabled)");
  hw::RTStubs::log_call(machine, "O.SET", "(native)");

  hw::Memory& mem = *machine.memory;

  int32_t E = bridge.entry_wsp();
  uint32_t Eu = static_cast<uint32_t>(E);
  int32_t F = E + 10;
  int32_t psr_body = bridge.entry_psr() | 0x8000;

  bridge.emulate_frame();                           // own WSAVS image [E+2..E+11]
  // walker WSSVR image at [E+12..E+23] (XJSR at wsp=E+10; frame F3=E+22;
  // its five locals [E+24..E+33] are never written — untouched residue)
  mem.write_wide(Eu + 12, psr_body << 16);
  mem.write_wide(Eu + 14, bridge.entry_ac(0));
  mem.write_wide(Eu + 16, bridge.entry_ac(1));
  mem.write_wide(Eu + 18, bridge.entry_ac(2));
  mem.write_wide(Eu + 20, F);
  mem.write_wide(Eu + 22, static_cast<int32_t>(OSET_XJSR_RET | (static_cast<uint32_t>(bridge.entry_carry()) << 31)));
  // walker result stores (EF00/EF02) + O.SET's record stores
  // (EE5B..EE5F) via the api (mv runs the live walker and lays all five
  // state cells; conforming implementations store the triple only).
  rt::error_handler().record_raise(machine,
    rt::SigRecord{bridge.entry_ac(0), bridge.entry_ac(1), bridge.entry_ac(2)});

  debug::Capture::native_footprint(machine);
  return bridge.native_return();
}

// O.SERROR (EE33): WSAVS; ac0 = -1 (ERROR); ac1 = 0; code = CALLER's ac2.
uint32_t o_serror(hw::Machine& machine) {
  if (nested_in_fallback(machine, "O.SERROR"))
    return hw::RTStubs::entry_address("O.SERROR");
  hw::RTBridge bridge(machine);
  if (rt::walker_gate_open(machine))
    return fallback(machine, "O.SERROR", "(native-fallback: gate wide 0x7017EEA0 > 0 — I?LINEID mode enabled)");
  hw::RTStubs::log_call(machine, "O.SERROR", "(native)");
  return signal_dispatch(machine, bridge, "O.SERROR", -1, 0, bridge.entry_ac(2), 0);
}

uint32_t o_sconve(hw::Machine& machine) { return shorthand(machine, "O.SCONVE", -1, 0x11611); }
uint32_t o_ssubsc(hw::Machine& machine) { return shorthand(machine, "O.SSUBSC", -1, 0x11612); }
uint32_t o_sfixed(hw::Machine& machine) { return shorthand(machine, "O.SFIXED", -2, 0x11606); }
uint32_t o_szerod(hw::Machine& machine) { return shorthand(machine, "O.SZEROD", -5, 0x11608); }
uint32_t o_soverf(hw::Machine& machine) { return shorthand(machine, "O.SOVERF", -3, 0x11607); }
uint32_t o_sunder(hw::Machine& machine) { return shorthand(machine, "O.SUNDER", -4, 0x11616); }

} // namespace emu_rt

// Strong rt::signal_has_handler (integration pass; overrides the weak
// conservative default in lib_error.cpp — see Project2/REPORT.md §3).
// ?LIB_ERROR raises catch-all: the EE38-instant args are type=-1,
// key2=0 (helper zeroes key2 for type<=0), so the found/not-found
// prediction is exactly select_frames(-1, 0). Pure reads. When the
// walker gate is open (never, in this binary — Project1/REPORT §4b)
// we cannot predict the dead branch's behavior: answer false, which
// makes ?LIB_ERROR fall back whole — safe by construction.
namespace rt {
bool signal_has_handler(hw::Machine& machine, int32_t code) {
  (void)code;
  if(rt::walker_gate_open(machine))
    return false;
  return rt::error_handler().has_handler(machine);   // select(-1, 0).found
}
}
