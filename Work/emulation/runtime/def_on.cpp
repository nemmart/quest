// src/runtime/def_on.cpp
//
// Derivation: docs/Project4/DERIVATION.md §6 (per-instruction listing,
// footprint maps, MOV.L# evidence). Writes are laid in EMULATED
// EXECUTION ORDER — later writes shadow earlier ones at the same
// addresses exactly as the emulated pushes would (o_signal.cpp
// convention), so this file audits against the disassembly top to
// bottom.
//
// Composition rules used here (the Project 4 pattern):
//   - Inner LEAF calls (O?AREA, R?SIGNAL's plain walk, P?DEFON's
//     type==2 C?INIT path) are laid AS RESIDUE — never dispatched —
//     so the whole composite ends in ONE native_return boundary at
//     DEF?ON's own return (0x7017EE40). Dispatching a sibling that
//     native_returns would end the clone batch at a mid-routine pc the
//     master never breaks at.
//   - The TRANSFER-capable inner call (the type>0 resignal, through
//     P?DEFON -> O?SIGNAL) IS dispatched via the registry: it ends in
//     native_transfer to game range, a boundary both engines share.
//   - Every branch that reaches ?FATAL (terminal), the [0x70000124]
//     restart, or an unobserved shape falls back WHOLE on pure reads.
#include "def_on.hpp"
#include "o_area.hpp"        // rt::o_area
#include "o_signal.hpp"      // rt::walker_gate_open (code-space gate, common)
#include "error_handler.hpp" // Project 8: sig record / resume flag / select
#include "../hw/Machine.hpp"
#include "../hw/Memory.hpp"
#include "../hw/RTBridge.hpp"
#include "../hw/RTStubs.hpp"
#include "../hw/NativeRegistry.hpp"
#include "../os/OSProcess.hpp"
#include "../debug/Capture.hpp"
#include <stdexcept>

namespace {

constexpr uint32_t A_P_DEFON   = 0x7017FD7Au;
constexpr uint32_t A_O_SIGNAL  = 0x7017EDEDu;
constexpr uint32_t RET_O_AREA  = 0x7017EF0Bu;
constexpr uint32_t RET_P_DEFON = 0x7017EF26u;
constexpr uint32_t RET_R_SIG   = 0x7017EF45u;
constexpr uint32_t A_FATAL     = 0x7017F036u;  // the L3 door (?FATAL entry)
constexpr uint32_t RET_FATAL   = 0x7017EF4Fu;  // ef4b LCALL's return pc
constexpr int32_t  FRAME_WALK_LIMIT = 1024;

int32_t rd(hw::Machine& m, uint32_t addr) {
  return static_cast<int32_t>(m.memory->read_wide(addr));
}

uint32_t fall_back(hw::Machine& machine, const char* why) {
  hw::RTStubs::log_call(machine, "DEF?ON", why);
  machine.rt_pending_return = static_cast<uint32_t>(machine.ac[3]);
  return hw::RTStubs::entry_address("DEF?ON");
}

bool translated(uint32_t addr) {
  return hw::RTStubs::active &&
         addr >= hw::RTStubs::start && addr < hw::RTStubs::stop &&
         hw::RTStubs::translated_bits[addr - hw::RTStubs::start] != 0;
}

// R?SIGNAL's walk outcome, pure reads (r_signal.cpp, from frame fp):
// true = plain return (chain reached 0), false = anomaly (restart/
// ?FATAL machinery).
bool r_signal_walk_returns(hw::Machine& machine, int32_t fp) {
  int32_t cursor = fp;
  int32_t guard = 0;
  for(;;) {
    if(++guard > FRAME_WALK_LIMIT)
      throw std::runtime_error("DEF?ON pre-walk: wfp chain exceeds 1024 (cycle?)");
    int32_t next = rd(machine, static_cast<uint32_t>(cursor) - 2);
    if(next == 0) return true;
    if(machine.frame_precedes(cursor, next)) return false;   // Ruling A (Project 12): master coordinates (Mapper frame_precedes)
    cursor = next;
  }
}

} // namespace

namespace rt {

bool def_on_would_run_native(hw::Machine& machine, int32_t type,
                             int32_t key2) {
  // Mirrors defq_on's decision phase (kept in the same file so the
  // gates and this predicate stay in lockstep; the dispatch re-checks
  // everything, so a disagreement shows up as the belt-and-braces
  // fallback, not a divergence... except count skew at a terminal —
  // hence: keep them identical).
  // Generation 3: consulted ONLY in the mv attic (HandlerMode::MV) —
  // o_signal.cpp's exhaustion gate retired the predicate for
  // native/check, where defq_on's terminal branches run native. This
  // mirrors defq_on's MV-mode gates, which are the Generation-2
  // originals, verbatim.
  if(type > 0) {
    if(type == 2)
      return true;                                   // C?INIT resume
    if(!translated(A_P_DEFON) || !translated(A_O_SIGNAL))
      return false;
    if(rt::walker_gate_open(machine))
      return false;
    int32_t new_type = (type == 6) ? -1 : 6;
    rt::SelectOutcome sel;
    rt::error_handler().select(machine, new_type, key2, sel);
    return sel.found;                                // resignal transfer
  }
  if(type == -1) {
    // The walk defq_on runs starts at ITS frame; at prediction time
    // that frame does not exist yet, but its chain prefix is known
    // and descending (each new frame sits above its caller), so the
    // outcome equals the walk from the CURRENT wfp.
    if(!r_signal_walk_returns(machine, machine.wfp))
      return false;
    // Resume flag = bit15 of the narrow at [area+0x16] = the sign bit
    // of the wide flag store at [wsb-0x2A]. O?SIGNAL's entry path has
    // already written this call's flag by prediction time (o_qsignal
    // stores before dispatching); the SHORTHAND bodies never store it,
    // so a shorthand-raised signal reads whatever the previous raise
    // left — faithful: the emulated bytes do exactly the same.
    return rt::error_handler().resume_flag(machine) < 0;   // wide sign = narrow bit15
  }
  return false;                                      // [-5..-2] resignal shape
}

} // namespace rt

namespace emu_rt {

uint32_t defq_on(hw::Machine& machine) {
  if(machine.rt_pending_return != 0) {
    hw::RTStubs::log_call(machine, "DEF?ON", "(native-skip: inside fallback span)");
    return hw::RTStubs::entry_address("DEF?ON");
  }

  hw::RTBridge bridge(machine);
  hw::Memory& mem = *machine.memory;
  int32_t E = bridge.entry_wsp();
  uint32_t Eu = static_cast<uint32_t>(E);
  int32_t F = E + 10;

  // ---------------- decision phase: pure reads only ----------------

  bool fatal_descent = false;   // Generation 3: type==-1, resume flag clear

  if(bridge.arg_count() != 0)
    return fall_back(machine, "(native-fallback: argc)");

  uint32_t area = rt::o_area(machine);
  rt::SigRecord rec = rt::error_handler().sig_record(machine);   // [area+2/4/6]
  int32_t type = rec.type;

  if(type > 0) {
    // The P?DEFON composite. Pre-run p_defon.cpp's own gates so the
    // dispatched sibling cannot fall back mid-flight; its type==2 path
    // is handled here by residue instead (see composition rules).
    if(machine.wsl > 0 && E + 78 > machine.wsl)   // P?DEFON's E'+40 from E'=E+38
      return fall_back(machine, "(native-fallback: headroom)");
    if(type != 2) {
      if(!translated(A_P_DEFON) || !translated(A_O_SIGNAL))
        return fall_back(machine, "(native-fallback: sibling untranslated)");
      if(rt::walker_gate_open(machine))
        return fall_back(machine, "(native-fallback: gate wide 0x7017EEA0 > 0 — I?LINEID mode enabled)");
      if(rt::handler_mode == rt::HandlerMode::MV) {
        // Generation-2 relic, mv attic verbatim: a no-handler resignal
        // runs to the terminal, and strict terminal counts forced the
        // whole chain to emulate. Generation 3 (native/check): the
        // resignal composes natively end to end — P?DEFON → O?SIGNAL →
        // exhaustion → this routine again — terminating at the native
        // ?FATAL descent below, a legal terminal crossing.
        int32_t new_type = (type == 6) ? -1 : 6;
        rt::SelectOutcome sel;
        rt::error_handler().select(machine, new_type, rec.key2, sel);
        if(!sel.found)
          return fall_back(machine, "(native-fallback: no handler — terminal-bound resignal, emulating)");
      }
    }
  } else if(type == -1) {
    if(machine.wsl > 0 && E + 44 > machine.wsl)
      return fall_back(machine, "(native-fallback: headroom)");
    if(!r_signal_walk_returns(machine, F))
      return fall_back(machine, "(native-fallback: R?SIGNAL wfp-chain anomaly, emulating)");
    // ef47..ef49: resume iff bit15 of narrow [area+0x16] is set;
    // clear -> ef4b ?FATAL, a terminal.
    if(rt::error_handler().resume_flag(machine) >= 0) {  // bit15 of narrow [area+0x16] = wide sign
      if(rt::handler_mode == rt::HandlerMode::MV)
        // Generation-2 relic, mv attic verbatim: strict terminal
        // counts required symmetric emulation to the door.
        return fall_back(machine, "(native-fallback: no-resume flag — ?FATAL terminal path, emulating)");
      fatal_descent = true;   // Generation 3: native ?FATAL descent below
    }
  } else {
    // type in [-5..-2] or other non-positive: the ef2c resignal-as-
    // ERROR path, then the R?SIGNAL tail. Never observed (every
    // recorded unhandled signal has been -1 or positive-by-
    // construction); symmetric emulation until a validation path
    // exists (METHOD.md sec. 9).
    return fall_back(machine, "(native-fallback: unobserved type<=0 resignal shape, emulating)");
  }

  hw::RTStubs::log_call(machine, "DEF?ON", "(native)");

  // ---------------- footprint phase ----------------

  int32_t psrD = bridge.entry_psr() | 0x8000;       // ovk=1 after WSAVS; ovr=0 since the XCALL

  // WSAVS 0x000A image [E+2..E+11]; locals [F+2..F+21].
  bridge.emulate_frame();

  // ef07 LCALL O?AREA,0: frame wide, then O?AREA's patched image —
  // instant ACs are DEF?ON's entry values (nothing ran before ef07).
  mem.write_wide(Eu + 32, psrD << 16);
  mem.write_wide(Eu + 34, static_cast<int32_t>(area));            // saved-ac0 slot, patched
  mem.write_wide(Eu + 36, bridge.entry_ac(1));
  mem.write_wide(Eu + 38, bridge.entry_ac(2));
  mem.write_wide(Eu + 40, F);
  mem.write_wide(Eu + 42, static_cast<int32_t>(RET_O_AREA | (static_cast<uint32_t>(bridge.entry_carry()) << 31)));

  // ef0b: local [F+12] = area.
  mem.write_wide(Eu + 22, static_cast<int32_t>(area));

  if(type > 0) {
    int32_t key2 = rec.key2;                     // [area+4]
    int32_t code = rec.code;                     // [area+6]
    // ef12..ef1a locals: [F+14]=key2, [F+16]=code, [F+18]=type.
    mem.write_wide(Eu + 24, key2);
    mem.write_wide(Eu + 26, code);
    mem.write_wide(Eu + 28, type);
    // ef1c..ef20 pushes (shadowing the O?AREA residue at [E+32..37]):
    mem.write_wide(Eu + 32, E + 28);                // &type-local
    mem.write_wide(Eu + 34, E + 26);                // &code-local
    mem.write_wide(Eu + 36, E + 24);                // &key2-local
    // ef22 LCALL P?DEFON,3: frame wide.
    mem.write_wide(Eu + 38, (psrD << 16) | 3);

    if(type == 2) {
      // P?DEFON's C?INIT path, laid as residue from its entry state
      // E'=E+38 (p_defon.cpp footprint map; its image's instant ACs:
      // ac0/ac1/ac2 = DEF?ON's values at ef22 = key2(ef16's load is
      // code... exactly: ac0=code after ef16, ac1=type after ef0e,
      // ac2=area), c = entry carry, wfp = F).
      mem.write_wide(Eu + 40, code);                // P?DEFON image [E'+2]: ac0 at ef22
      mem.write_wide(Eu + 42, type);                //   ac1
      mem.write_wide(Eu + 44, static_cast<int32_t>(area));   // ac2
      mem.write_wide(Eu + 46, F);                   //   wfp (DEF?ON's frame)
      mem.write_wide(Eu + 48, static_cast<int32_t>(RET_P_DEFON | (static_cast<uint32_t>(bridge.entry_carry()) << 31)));
      // Its XPEF @[arg1 slot] and the C?INIT frame wide + image
      // ([E'+26..E'+39] = [E+64..E+77]):
      int32_t Fp = E + 48;                          // P?DEFON's frame ptr E'+10
      mem.write_wide(Eu + 64, E + 24);              // XPEF @arg1: pushes arg1's stored pointer, = E+24 (&key2-local)
      mem.write_wide(Eu + 66, (psrD << 16) | 1);    // LCALL C?INIT,1 frame wide
      mem.write_wide(Eu + 68, 2);                   // C?INIT image: ac0 = type (=2) at fd83
      mem.write_wide(Eu + 70, type);                //   ac1 (unchanged since P?DEFON entry)
      mem.write_wide(Eu + 72, static_cast<int32_t>(area));   // ac2
      mem.write_wide(Eu + 74, Fp);                  //   wfp = P?DEFON's frame
      mem.write_wide(Eu + 76, static_cast<int32_t>(0x7017FD87u | (static_cast<uint32_t>(bridge.entry_carry()) << 31)));
      debug::Capture::native_footprint(machine);
      return bridge.native_return();                // one boundary: 0x7017EE40
    }

    // Resignal: dispatch the sibling with the machine exactly as the
    // ef22 LCALL leaves it (p_defon.cpp then simulates ITS O?SIGNAL
    // LCALL and transfers; its fallbacks are pre-checked above).
    machine.wsp = E + 38;
    machine.wfp = F;
    machine.ovk = 1;
    machine.ovr = 0;
    machine.ac[0] = code;                           // ef16's load is live at ef22
    machine.ac[1] = type;
    machine.ac[2] = static_cast<int32_t>(area);
    machine.ac[3] = static_cast<int32_t>(RET_P_DEFON);
    // machine.c unchanged: entry carry, as emulation leaves it.
    debug::Capture::native_footprint(machine);
    return machine.process->native_registry.lookup(A_P_DEFON)(machine);
  }

  // type == -1: ef27 load, ef41 LCALL R?SIGNAL,0 — laid as residue
  // (plain-walk outcome pre-checked): frame wide shadows [E+32,33],
  // image shadows [E+34..43]. Instant ACs at ef41: ac0 = [area+2] = -1
  // (ef27), ac1 = type = -1 (ef0e), ac2 = area. This residue is SHARED
  // between the resume exit (ef4a WRTN) and the Generation-3 ?FATAL
  // descent (ef4b LCALL): the two branches write nothing different —
  // ?FATAL's LCALL frame word lands at the same [E+32] with the same
  // value ((psrD<<16)|0, argc 0 both) — so they differ only in the
  // machine state they exit with.
  mem.write_wide(Eu + 32, psrD << 16);
  mem.write_wide(Eu + 34, -1);
  mem.write_wide(Eu + 36, -1);
  mem.write_wide(Eu + 38, static_cast<int32_t>(area));
  mem.write_wide(Eu + 40, F);
  mem.write_wide(Eu + 42, static_cast<int32_t>(RET_R_SIG | (static_cast<uint32_t>(bridge.entry_carry()) << 31)));

  if(fatal_descent) {
    // Generation 3 (docs/CheckerHistory.md): the L2→L3 descent runs
    // NATIVE to the door and transfers there with the contracted state
    // (Contract §3.13, type==-1 exit row). The emulated instants:
    //   ef41 R?SIGNAL returns — WRTN restores ac0=-1, ac1=-1, ac2=area,
    //        c = entry carry, ac3 = wfp = F, wsp = pre-LCALL top;
    //   ef45 XWLDA 2,[ac3+0xC]  -> ac2 = [F+12] = area (same value);
    //   ef47 XNLDA 0,[ac2+0x16] -> ac0 = sign-extended narrow
    //        [area+0x16] = high word of the flag wide at [wsb-0x2A]
    //        (arithmetic >>16 of the wide; sign bit clear on this
    //        branch, or we'd have resumed);
    //   ef49 MOV.L# 0,0,SZC     -> pure test, no state written;
    //   ef4b LCALL [?FATAL],0   -> frame word (psrD<<16)|0 at [E+32]
    //        (already there), wsp = E+32, ac3 = 0x7017EF4F.
    // The terminal pair then compares pc + the full register file at
    // the door (Lockstep::compare_pair); counts are exempt because the
    // clone arrives on a native span (Machine::run_steps native_break).
    machine.ac[0] = rt::error_handler().resume_flag(machine) >> 16;
    machine.ac[1] = -1;
    machine.ac[2] = static_cast<int32_t>(area);
    machine.ac[3] = static_cast<int32_t>(RET_FATAL);
    machine.c = bridge.entry_carry();
    machine.ovk = 1;
    machine.ovr = 0;
    machine.wfp = F;
    machine.wsp = E + 32;
    debug::Capture::native_footprint(machine);
    return hw::RTBridge::native_transfer(machine, A_FATAL);
  }

  // Resume exit: ef45..ef49 are loads and a no-load skip test (MOV.L#
  // leaves ALL machine state untouched — NovaCompute N=1 path); ef4a WRTN.
  debug::Capture::native_footprint(machine);
  return bridge.native_return();
}

} // namespace emu_rt
