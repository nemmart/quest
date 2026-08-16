// src/runtime/p_defon.cpp
//
// Derivation, byte-for-byte residue map, and instruction-semantics
// evidence: docs/Project4/DERIVATION.md §3. Memory writes are laid in
// EMULATED EXECUTION ORDER so this file audits against the disassembly
// top to bottom (the o_signal.cpp convention).
//
// Body (entry state: wsp=E, frame wide at [E,E+1], args below E):
//   7017fd7a WSAVS 0x0007;             image [E+2..E+11], F=E+10,
//                                      locals [F+2..F+15], wsp=E+24
//   7017fd7c XWLDA 0,@[ac3-0x10];      ac0 = *arg3 = type
//   7017fd7e WSEQI 0,2;  WBR ->fd88 if type != 2
//     -- type == 2 (C?INIT path) --
//   7017fd81 XPEF @[ac3-0xC];          push arg1 pointer  [E+26,27]
//   7017fd83 LCALL C?INIT,1;           frame wide         [E+28,29]
//            C?INIT = WSAVS 0; WRTN    image              [E+30..39]
//   7017fd87 WRTN;
//     -- type != 2 --
//   7017fd88 XWLDA 0,@[ac3-0x10];      ac0 = type (again)
//   7017fd8a WSEQI 0,6;  WBR ->fd91 if type != 6
//   7017fd8d WADC 0,0;                 ac0 = -1, c = 1 (EagleCompute
//                                      add(~src,dst): carry-out 1,
//                                      ovr contribution 0 — §3.4)
//   7017fd8e XWSTA 0,[ac3+0xC];  WBR ->fd95      local [F+12] = -1
//   7017fd91 NLDAI 6,0;                ac0 = 6 (no flag effects)
//   7017fd93 XWSTA 0,[ac3+0xC];        local [F+12] = 6
//   7017fd95 XPEF @[ac3-0xE];          push arg2 pointer  [E+26,27]
//   7017fd97 XPEF @[ac3-0xC];          push arg1 pointer  [E+28,29]
//   7017fd99 XPEF [ac3+0xC];           push F+12          [E+30,31]
//   7017fd9b LCALL O?SIGNAL,3;         frame wide         [E+32,33]
//   7017fd9f WRTN;                     (reached only if a handler
//                                      RESUMES back through O?SIGNAL)
#include "p_defon.hpp"
#include "o_signal.hpp"      // rt::walker_gate_open (code-space gate, common)
#include "error_handler.hpp" // Project 8: handler selection via the api
#include "../hw/Machine.hpp"
#include "../hw/Memory.hpp"
#include "../hw/RTBridge.hpp"
#include "../hw/RTStubs.hpp"
#include "../hw/NativeRegistry.hpp"
#include "../os/OSProcess.hpp"
#include "../debug/Capture.hpp"

namespace {

constexpr uint32_t A_O_SIGNAL   = 0x7017EDEDu;
constexpr uint32_t A_C_INIT_RET = 0x7017FD87u;  // LCALL C?INIT return pc
constexpr uint32_t A_OSIG_RET   = 0x7017FD9Fu;  // LCALL O?SIGNAL return pc

uint32_t fall_back(hw::Machine& machine, const char* why) {
  hw::RTStubs::log_call(machine, "P?DEFON", why);
  machine.rt_pending_return = static_cast<uint32_t>(machine.ac[3]);
  return hw::RTStubs::entry_address("P?DEFON");
}

bool o_signal_translated() {
  return hw::RTStubs::active &&
         A_O_SIGNAL >= hw::RTStubs::start && A_O_SIGNAL < hw::RTStubs::stop &&
         hw::RTStubs::translated_bits[A_O_SIGNAL - hw::RTStubs::start] != 0;
}

} // namespace

namespace emu_rt {

uint32_t pq_defon(hw::Machine& machine) {
  if(machine.rt_pending_return != 0) {
    // Nested-in-fallback guard. Today this is every live path: the
    // sole caller DEF?ON is a terminal entry the clone reaches only
    // inside a whole-chain fallback span (SharedProtocol.md), so this
    // wrapper is dormant until the DEF?ON lift moves the detach point
    // deeper (Layering.md validation hook).
    hw::RTStubs::log_call(machine, "P?DEFON", "(native-skip: inside fallback span)");
    return hw::RTStubs::entry_address("P?DEFON");
  }

  hw::RTBridge bridge(machine);
  hw::Memory& mem = *machine.memory;
  int32_t E = bridge.entry_wsp();
  uint32_t Eu = static_cast<uint32_t>(E);
  int32_t F = E + 10;

  // --- Decision on PURE READS, before any store (SharedProtocol.md
  // terminal-composition rule) ---

  if(bridge.arg_count() != 3)
    // Sole static call site passes 3; symmetric emulation otherwise.
    return fall_back(machine, "(native-fallback: argc)");

  // WSAVS headroom + deepest wide_push ([E+32,33] signal branch,
  // [E+38,39] C?INIT branch): fall back into faithful emulation of the
  // stack-fault vectoring rather than replicate it (METHOD.md §13).
  if(machine.wsl > 0 && E + 40 > machine.wsl)
    return fall_back(machine, "(native-fallback: headroom)");

  int32_t type = bridge.arg_wide(3);

  if(type != 2) {
    // Resignal branch: composes with native O?SIGNAL through the
    // registry (?DEFAULT_ERROR_HANDLER precedent, lib_error.cpp).
    // Every condition that would make the sibling fall back must be
    // pre-checked HERE, while falling back whole is still possible:
    if(!o_signal_translated())
      return fall_back(machine, "(native-fallback: o-signal-untranslated)");
    if(rt::walker_gate_open(machine))
      return fall_back(machine, "(native-fallback: gate wide 0x7017EEA0 > 0 — I?LINEID mode enabled)");
    int32_t new_type = (type == 6) ? -1 : 6;
    rt::SelectOutcome sel;
    rt::error_handler().select(machine, new_type, bridge.arg_wide(1), sel);
    if(rt::handler_mode == rt::HandlerMode::MV && !sel.found)
      // Generation-2 relic, mv attic verbatim: a no-handler resignal
      // heads to the terminal, which strict terminal counts made
      // unpairable natively. Generation 3 (native/check): the sibling
      // O?SIGNAL handles its own exhaustion natively — dispatch DEF?ON,
      // whose branches end at the native ?FATAL descent (def_on.cpp) —
      // so a no-handler resignal is an ordinary composition.
      return fall_back(machine, "(native-fallback: no handler — DEF?ON terminal path, emulating for terminal pairing)");

    hw::RTStubs::log_call(machine, "P?DEFON", "(native)");

    int32_t psr_body = bridge.entry_psr() | 0x8000;   // ovk=1 after WSAVS; ovr=0 since the LCALL

    // --- own WSAVS image [E+2..E+11]; locals [F+2..F+15] untouched
    //     except [F+12] below ---
    bridge.emulate_frame();
    // --- XWSTA 0,[ac3+0xC]: the resignal type ---
    mem.write_wide(Eu + 22, new_type);                // F+12 = E+22
    // --- the three XPEF pushes (wides, residue until the LCALL) ---
    mem.write_wide(Eu + 26, static_cast<int32_t>(bridge.arg_pointer(2)));
    mem.write_wide(Eu + 28, static_cast<int32_t>(bridge.arg_pointer(1)));
    mem.write_wide(Eu + 30, F + 12);
    // --- LCALL O?SIGNAL,3: frame wide, then machine state exactly as
    //     the emulated LCALL would leave it for the callee ---
    mem.write_wide(Eu + 32, (psr_body << 16) | 3);
    machine.wsp = E + 32;
    machine.wfp = F;
    machine.ovk = 1;
    machine.ovr = 0;
    machine.ac[0] = new_type;
    // ac1, ac2 untouched: still the entry values, as emulation leaves them.
    machine.ac[3] = static_cast<int32_t>(A_OSIG_RET);
    machine.c = (type == 6) ? 1 : bridge.entry_carry();   // WADC 0,0 sets c=1; NLDAI path keeps entry c

    debug::Capture::native_footprint(machine);

    // Dispatch the sibling. Gate pre-checked (and, in the mv attic,
    // sel.found too), so its own fallbacks are unreachable; it lays its
    // footprint from wsp=E+32 and native_transfers to the handler — or,
    // Generation 3, composes through DEF?ON to the native ?FATAL
    // descent on exhaustion. If it ever does
    // return a fallback pc anyway, the machine state above is exactly
    // the emulated post-LCALL state, so emulating from that pc stays
    // coherent (the lib_error.cpp pc3 convention).
    return machine.process->native_registry.lookup(A_O_SIGNAL)(machine);
  }

  // --- type == 2: LCALL C?INIT(arg1) — body is WSAVS 0; WRTN — then
  //     a normal return. Pure residue: nothing observable changes. ---
  hw::RTStubs::log_call(machine, "P?DEFON", "(native)");

  int32_t psr_body = bridge.entry_psr() | 0x8000;
  bridge.emulate_frame();                             // [E+2..E+11]
  // XPEF @[ac3-0xC]: push the arg1 pointer
  mem.write_wide(Eu + 26, static_cast<int32_t>(bridge.arg_pointer(1)));
  // LCALL C?INIT,1: frame wide
  mem.write_wide(Eu + 28, (psr_body << 16) | 1);
  // C?INIT's WSAVS 0x0000 image: ac0 = type (the fd7c load), ac1/ac2 =
  // entry values (nothing between entry and fd83 touches them), wfp =
  // this frame, ret = fd87 with the entry carry (nothing set or
  // cleared c on this path — §3.4).
  mem.write_wide(Eu + 30, type);
  mem.write_wide(Eu + 32, bridge.entry_ac(1));
  mem.write_wide(Eu + 34, bridge.entry_ac(2));
  mem.write_wide(Eu + 36, F);
  mem.write_wide(Eu + 38, static_cast<int32_t>(A_C_INIT_RET | (static_cast<uint32_t>(bridge.entry_carry()) << 31)));

  debug::Capture::native_footprint(machine);
  return bridge.native_return();
}

} // namespace emu_rt
