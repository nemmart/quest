#include "EagleStack.hpp"
#include "Machine.hpp"
#include "NativeRegistry.hpp"
#include "RTStubs.hpp"
#include "Lockstep.hpp"
#include "AddressBook.hpp"
#include "../os/Trace.hpp"
#include "../debug/CallStack.hpp"
#include "../debug/SymbolTable.hpp"
#include "../os/OSTask.hpp"
#include "../os/OSProcess.hpp"
#include <cstring>




namespace hw {

// `gcalls` trace (Project 13): one line per LCALL/XCALL whose target lies
// in the GAME range [QUEST, ?CHAR_TO_UNSIGNED) — the routine-coverage
// instrument, independent of the M4a redirect (per live routine, the gcalls
// count must equal the redirect WSAVS count). Diagnostic only: emitted at the
// dispatch site in BOTH roles; touches no pairing, count, or batch state.
static void trace_gcall(Machine& machine, int32_t site, int32_t target, int32_t arguments) {
  if(!os::Trace::enabled("gcalls"))
    return;
  static uint32_t game_lo=0;
  if(game_lo==0) {
    game_lo=machine.symbols ? machine.symbols->address_for_name("QUEST") : 0xFFFFFFFFu;
    if(game_lo==0xFFFFFFFFu) game_lo=0x7015C005u;   // no symbol table: the known layout
  }
  uint32_t t=static_cast<uint32_t>(target);
  if(t<game_lo || t>=RTStubs::start)
    return;
  std::string name=machine.symbols ? machine.symbols->name_for_address(t) : std::string();
  char buf[160];
  snprintf(buf, sizeof(buf), "target=%08X %-20s argc=%d site=%08X",
           t, name.empty() ? "-" : name.c_str(), arguments & 0x7FFF, static_cast<uint32_t>(site));
  os::Trace::line("gcalls", machine.process ? machine.process->instance_label : std::string("?"), buf);
}
using namespace debug;
using namespace os;

void EagleStack::setup(uint32_t opcode, const std::string& name, const std::string& fmt, int32_t op) {
  Instruction::setup(opcode, name, fmt, op);
  opcode=opcode>>11;
  AA=opcode & 0x03;
  opcode=opcode>>2;
  XX=opcode & 0x03;
}

// LAYERING NOTE (docs/Layering.md ruling 6, FINAL): the boot stack
// fault is load-bearing — the image ships wsp==wsl and the first push
// faults into I.INIT ("fault your way into init"). That signature is
// provable from the .PR preambles and gates the ONLY permitted
// vectoring below; every other stack fault is a C++ throw (not an MV
// fault) by ruling. History: "always vector" was original; "any limit
// kills" was tried Aug 11 and tripped 3x at launch (VOID); the
// wsp==wsl gate landed Aug 13. I.SFCON is wired in slot 0x1BB but
// deliberately unreachable under this rule — if the throw below ever
// fires in play, that is the moment to reconsider.
uint32_t EagleStack::handle_overflow(Machine& machine, uint32_t address, uint32_t next_instruction) {
  // RULING (Layering.md ruling 6, implemented Aug 13): wsp==wsl at
  // fault is the image's PROVABLE boot-probe signature (both .PR
  // preambles ship wsp==wsl; the first push faults into I.INIT via
  // slot 0x1B8 — "fault your way into init"). That one fault vectors
  // faithfully below. ANY other stack fault throws RIGHT HERE — no
  // fault machinery: post-detach it ends the death informatively;
  // symmetric cases throw identically on both engines (agreed death);
  // a master-only throw inside a native span surfaces as an
  // exception-mismatch divergence (upgrade to forced shutdown is a
  // later task). This deliberately replaces the wired I.SFCON path.
  if(machine.wsp != machine.wsl) {
    char buf[112];
    snprintf(buf, sizeof(buf),
             "STACK LIMIT at %08X (wsp=%08X wsl=%08X) — non-boot stack faults are terminal by ruling",
             address, machine.wsp, machine.wsl);
    // Master (or server): no counterpart can mirror this — take the
    // whole world down deliberately, WITH data write-back (unlike DERR,
    // a stack limit does not imply corrupted game state). The clone's
    // own throw stays a raw crash: a clone-side fault is an our-code
    // bug and deserves maximum forensics.
    if(Lockstep::enabled && machine.lockstep_role != Lockstep::CLONE)
      Lockstep::abort_world(buf, &machine, /*save=*/true);
    throw std::runtime_error(buf);
  }

  uint32_t wsl=machine.wsl;

  if(machine.task->stack_fault_handler==0)
    throw std::runtime_error("Stack handler set to 0x0000");
  if(machine.task->stack_fault_handler==static_cast<int32_t>(0xFFFF))
    throw std::runtime_error("Stack handler set to 0xFFFF");

  machine.wsl=machine.wsl+100;
  machine.wide_push(machine.get_psr()<<16);
  machine.wide_push(machine.ac[0]);
  machine.wide_push(machine.ac[1]);
  machine.wide_push(machine.ac[2]);
  machine.wide_push(machine.ac[3]);
  machine.wide_push(address | (machine.c<<31));
  machine.ovk=0;
  machine.ovr=0;
  machine.ires=0;
  machine.wsp=machine.wsp | 0x80000000;
  machine.wsl=wsl | 0x80000000;
  machine.ac[0]=address;
  machine.ac[1]=1;
  return copy_segment(address, machine.task->stack_fault_handler);
}

// Stack-claim zeroing — RULING 8 (docs/Layering.md), the project's
// FOURTH deliberate infidelity, user-ratified Aug 14 2026. Real
// hardware only RESERVED claimed stack space (DG Instruction
// Dictionary, archived pages: "reserving space" — a period cost
// decision, not a semantic commitment). This emulator ZEROES every
// word a claim exposes, so read-before-write locals see deterministic
// 0 on both engines — and keep seeing 0 under M4's zero-initializing
// prologues, permanently. `-zero=none` is the bit-faithful attic.
//
// A "claim" = any wsp increase exposing unwritten words. Pushes write
// their words and are NOT claims. Claimed words for old_wsp→new_wsp
// are [old_wsp+2, new_wsp+2): wsp points AT the top wide (wide_push
// pre-increments), so the first unwritten word is old_wsp+2 and the
// claim's last word is new_wsp+1.
//
// The sign guard: a claim never crosses the 0x80000000 boot-fault
// marker (handle_overflow tags wsp with it during the deliberate
// boot overflow). A sign-crossing "raise" is stack re-basing
// (I.INIT installing the real stack) or the marker itself — not a
// claim — and, under copy_segment's 28-bit masking, a sign-crossing
// zero loop would sweep the whole segment.
static void zero_claim(Machine& machine, int32_t old_wsp, int32_t new_wsp) {
  if(new_wsp <= old_wsp) return;
  if((old_wsp ^ new_wsp) & static_cast<int32_t>(0x80000000)) return;
  for(int32_t w = old_wsp + 2; w < new_wsp + 2; w++)
    machine.memory->write_word(Machine::copy_segment(machine.pc, w), 0);
}

uint32_t EagleStack::execute(Machine& machine, uint32_t address, uint32_t opcode) {
  int32_t    frame_size, resolved, arguments, value;
  NativeFunc native;

  switch(oper) {
   case XCALL:
    resolved=machine.eagle_x_resolve_indirect(copy_segment(address, address+1), AA);
    arguments=machine.memory->read_word(copy_segment(address, address+2));
    if((arguments & 0x8000)==0)
      machine.wide_push((machine.get_psr()<<16)|arguments);
    else
      machine.wide_push(arguments & 0x7FFF);
    machine.ac[3]=copy_segment(address, address+3);
    machine.ovr=0;
    if(static_cast<uint32_t>(resolved)==0x30000000) {
      native=machine.process->native_registry.lookup(0x30000000);
      if(native)
        return native(machine);
      return resolved;
    }
    else if(get_segment(resolved)!=7)
      throw std::runtime_error("ILLEGAL CALL");
    machine.call_stack->call(resolved, machine.ac[3], address, arguments);
    trace_gcall(machine, address, resolved, arguments);
    native=machine.process->native_registry.lookup(static_cast<uint32_t>(resolved));
    if(native) {
      // Nested-in-fallback guard (Project 1 finding, centralized at
      // integration): inside an outer fallback span the clone must
      // re-emulate EVERYTHING, exactly as the master (whose registry is
      // empty) does — running an inner native here would skew the
      // span's instruction counts, which are compared when the span
      // ends at a terminal pair. Do not re-arm rt_pending_return.
      if(machine.rt_pending_return!=0)
        return resolved;
      if(RTStubs::defer_dispatch(static_cast<uint32_t>(resolved))) {
        // Crossings-only checker: an L1→L2 crossing into translated L2.
        // Defer the native call so the batch breaks AT the entry pc —
        // the crossing rendezvous (argument state compared) — and the
        // implementation runs on resume (Machine::pending_native).
        machine.pending_native=native;
        return resolved;
      }
      return native(machine);
    }
    return resolved;

   case LCALL:
    resolved=machine.eagle_l_resolve_indirect(copy_segment(address, address+1), AA);
    arguments=machine.memory->read_word(copy_segment(address, address+3));
    if((arguments & 0x8000)==0)
      machine.wide_push((machine.get_psr()<<16)|arguments);
    else
      machine.wide_push(arguments & 0x7FFF);
    machine.ac[3]=copy_segment(address, address+4);
    machine.ovr=0;
    if(static_cast<uint32_t>(resolved)==0x30000000) {
      native=machine.process->native_registry.lookup(0x30000000);
      if(native)
        return native(machine);
      return resolved;
    }
    else if(get_segment(resolved)!=7)
      throw std::runtime_error("ILLEGAL CALL");
    machine.call_stack->call(resolved, machine.ac[3], address, arguments);
    trace_gcall(machine, address, resolved, arguments);
    native=machine.process->native_registry.lookup(static_cast<uint32_t>(resolved));
    if(native) {
      // Nested-in-fallback guard (Project 1 finding, centralized at
      // integration): inside an outer fallback span the clone must
      // re-emulate EVERYTHING, exactly as the master (whose registry is
      // empty) does — running an inner native here would skew the
      // span's instruction counts, which are compared when the span
      // ends at a terminal pair. Do not re-arm rt_pending_return.
      if(machine.rt_pending_return!=0)
        return resolved;
      if(RTStubs::defer_dispatch(static_cast<uint32_t>(resolved))) {
        // Crossings-only checker: an L1→L2 crossing into translated L2.
        // Defer the native call so the batch breaks AT the entry pc —
        // the crossing rendezvous (argument state compared) — and the
        // implementation runs on resume (Machine::pending_native).
        machine.pending_native=native;
        return resolved;
      }
      return native(machine);
    }
    return resolved;

   case WSAVR: case WSAVS:
    frame_size=machine.memory->read_word(copy_segment(address, address+1));
    // M4a redirect (docs/M4aDesign.md §4; clone only, keyed by pc through the
    // address book): the caller's args + frame word are COPIED into the
    // routine's fixed area, the five restore wides are written into the
    // area instead of the stack, wfp/ac3 point into the area, and the real
    // wsp is left where the caller's LCALL put it. Not in the book (or no
    // book): exact stock behavior below.
    {
      // Gate = CONFIGURATION (Mapper.md §3): the master's mapper has no
      // book, so entry_for_pc is null there — no role query here.
      BookEntry* book_entry=machine.mapper.entry_for_pc(address);
      if(book_entry) {
        if(book_entry->live) {   // routines are not re-entrant — the dynamic tripwire
          char buf[160];
          snprintf(buf, sizeof(buf), "AREA: re-entry of %s at %08X while its area frame is live",
                   book_entry->name.c_str(), address);
          if(Lockstep::enabled) Lockstep::abort_world(buf, &machine, /*save=*/false);
          throw std::runtime_error(buf);
        }
        // Master-side overflow symmetry: the master's WSAVS would fault on
        // its stack; the clone's real stack does not grow here, so test the
        // shadow value (a fault here is a non-boot fault: terminal by ruling).
        if(machine.wsl>0 && machine.shadow_wsp()+10+frame_size*2>machine.wsl)
          return handle_overflow(machine, address, copy_segment(address, address+2));
        int32_t frame_word=machine.memory->read_wide(copy_segment(address, machine.wsp));
        int32_t argc=frame_word & 0x7FFF;
        if(argc>book_entry->max_argc) {
          char buf[160];
          snprintf(buf, sizeof(buf), "AREA: %s called with argc %d > book max %d", book_entry->name.c_str(), argc, book_entry->max_argc);
          if(Lockstep::enabled) Lockstep::abort_world(buf, &machine, /*save=*/false);
          throw std::runtime_error(buf);
        }
        int32_t W=machine.wsp;                                   // real: the LCALL frame word
        int32_t area_wfp=static_cast<int32_t>(book_entry->wfp_base);
        // args + frame word, verbatim, at the same offsets from wfp
        for(int32_t k=0; k<2*argc+2; k++)
          machine.memory->write_word(static_cast<uint32_t>(area_wfp-10-2*argc+k),
                                     machine.memory->read_word(copy_segment(address, W-2*argc+k)));
        // the five restore wides, into the area (real stack untouched)
        machine.memory->write_wide(static_cast<uint32_t>(area_wfp-8), machine.ac[0]);
        machine.memory->write_wide(static_cast<uint32_t>(area_wfp-6), machine.ac[1]);
        machine.memory->write_wide(static_cast<uint32_t>(area_wfp-4), machine.ac[2]);
        machine.memory->write_wide(static_cast<uint32_t>(area_wfp-2), machine.wfp);
        machine.memory->write_wide(static_cast<uint32_t>(area_wfp+0), machine.ac[3] | (machine.c<<31));
        machine.ac[3]=area_wfp;
        machine.wfp=area_wfp;
        // WSAVS space [wfp+2, wfp+2+2*frame): zeroed per ruling 8 (the
        // master zeroes its claim; the area keeps prior contents otherwise)
        if(machine.zero_claims)
          for(int32_t w=area_wfp+2; w<area_wfp+2+2*frame_size; w++)
            machine.memory->write_word(static_cast<uint32_t>(w), 0);
        machine.ovk=(oper==WSAVR)?0:1;
        machine.call_stack->augment(machine.wfp, frame_size);
        // Mutation site 1 of 3 (Mapper.md §3): push the live record — the
        // mapper computes master_wfp/shift, asserts I2/I5 + the main-task
        // assert + extent-fits-block, sets the live flag, and traces.
        machine.mapper.push_record(machine, book_entry, address, W, argc, frame_size);
        return copy_segment(address, address+2);
      }
    }
    if(machine.wsl>0 && machine.wsp+10+frame_size*2>machine.wsl)
      return handle_overflow(machine, address, copy_segment(address, address+2));
    machine.wide_push(machine.ac[0]);
    machine.wide_push(machine.ac[1]);
    machine.wide_push(machine.ac[2]);
    machine.wide_push(machine.wfp);
    machine.wide_push(machine.ac[3] | (machine.c<<31));
    machine.ac[3]=machine.wsp;
    machine.wfp=machine.wsp;
    machine.wsp=machine.wsp+frame_size*2;
    // TRIPWIRE — deliberate infidelity #4 (ruling 8, docs/Layering.md):
    // the DG manual says WSAVS is "reserving" this space; we ZERO it.
    // The license is ruling 8 (user-ratified Aug 14 2026); the
    // bit-faithful escape is `-zero=none`; the empirical probe for
    // load-bearing 1988 garbage is `-zero=clone` (docs/Project10).
    if(machine.zero_claims)
      zero_claim(machine, machine.wfp, machine.wsp);
    machine.ovk=(oper==WSAVR)?0:1;
    machine.call_stack->augment(machine.wfp, frame_size);
    return copy_segment(address, address+2);

   case WSSVR: case WSSVS:
    frame_size=machine.memory->read_word(address+1);
    if(machine.wsl>0 && machine.wsp+12+frame_size*2>machine.wsl)
      return handle_overflow(machine, address, copy_segment(address, address+2));
    if((machine.memory->read_word(machine.ac[3]-3) & 0xE7FF)==0xA6E9)
      machine.call_stack->call(address, machine.ac[3], machine.ac[3]-3, 0);
    else if((machine.memory->read_word(machine.ac[3]-2) & 0xE7FF)==0xC619)
      machine.call_stack->call(address, machine.ac[3], machine.ac[3]-2, 0);
    else
      machine.call_stack->call(address, machine.ac[3], -1, 0);
    machine.wide_push(machine.get_psr()<<16);
    machine.wide_push(machine.ac[0]);
    machine.wide_push(machine.ac[1]);
    machine.wide_push(machine.ac[2]);
    machine.wide_push(machine.wfp);
    machine.wide_push(machine.ac[3] | (machine.c<<31));
    machine.ac[3]=machine.wsp;
    machine.wfp=machine.wsp;
    machine.wsp=machine.wsp+frame_size*2;
    if(machine.zero_claims)   // ruling 8 — see the WSAVS tripwire above
      zero_claim(machine, machine.wfp, machine.wsp);
    machine.ovr=0;
    machine.ovk=(oper==WSSVR)?0:1;
    machine.call_stack->augment(machine.wfp, frame_size);
    return copy_segment(address, address+2);

   case WRTN: {
    // M4a: stock sequence; if wfp was an AREA address the pops come from
    // the area image (same offsets), and only the final wsp needs the
    // fixup — Machine::area_wrtn_fixup, gated on the range test.
    int32_t pre_wfp=machine.wfp;
    machine.wsp=machine.wfp;
    value=machine.wide_pop();
    machine.wfp=machine.wide_pop();
    machine.ac[2]=machine.wide_pop();
    machine.ac[1]=machine.wide_pop();
    machine.ac[0]=machine.wide_pop();
    frame_size=machine.wide_pop();
    machine.ac[3]=machine.wfp;
    machine.set_psr(static_cast<uint32_t>(frame_size)>>16);
    frame_size=frame_size & 0x7FFF;
    machine.wsp=machine.wsp-2*frame_size;
    machine.c=static_cast<uint32_t>(value)>>31;
    machine.area_wrtn_fixup(pre_wfp);
    machine.call_stack->call_return(value & 0x7FFFFFFF);
    return value & 0x7FFFFFFF;
   }

   case WPOPB:
    value=machine.wide_pop();
    machine.ac[3]=machine.wide_pop();
    machine.ac[2]=machine.wide_pop();
    machine.ac[1]=machine.wide_pop();
    machine.ac[0]=machine.wide_pop();
    frame_size=machine.wide_pop();
    machine.set_psr(static_cast<uint32_t>(frame_size)>>16);
    machine.wsp=machine.wsp-(frame_size & 0x7FFF)*2;
    machine.c=static_cast<uint32_t>(value)>>31;
    return value & 0x7FFFFFFF;

   case LDASP:
    machine.ac[AA]=machine.wsp;
    return copy_segment(address, address+1);

   case STASP:
    value=machine.wsp;
    machine.wsp=machine.ac[AA];
    // Ruling 8, defensive completeness: the known live STASP (the
    // I.GOTO landing stub) LOWERS wsp; a raise exposes unwritten
    // words and is a claim. zero_claim's sign guard exempts stack
    // RE-BASING (I.INIT boot choreography) from this rule.
    if(machine.zero_claims)
      zero_claim(machine, value, machine.wsp);
    return copy_segment(address, address+1);

   case LDAFP:
    machine.ac[AA]=machine.wfp;
    return copy_segment(address, address+1);

   case STAFP:
    machine.wfp=machine.ac[AA];
    return copy_segment(address, address+1);

   case LDASB:
    machine.ac[AA]=machine.wsb;
    return copy_segment(address, address+1);

   case STASB:
    machine.wsb=machine.ac[AA];
    return copy_segment(address, address+1);

   case LDASL:
    machine.ac[AA]=machine.wsl;
    return copy_segment(address, address+1);

   case STASL:
    machine.wsl=machine.ac[AA];
    return copy_segment(address, address+1);

   case LDATS:
    machine.ac[AA]=machine.memory->read_wide(machine.wsp);
    return copy_segment(address, address+1);

   case STATS:
    machine.memory->write_wide(machine.wsp, machine.ac[AA]);
    return copy_segment(address, address+1);

   case ISZTS: case DSZTS:
    value=machine.memory->read_wide(copy_segment(address, machine.wsp));
    if(oper==ISZTS) value++;
    if(oper==DSZTS) value--;
    machine.memory->write_wide(copy_segment(address, machine.wsp), value);
    if(value==0) return copy_segment(address, address+2);
    return copy_segment(address, address+1);

   case WMSP:
    value=machine.wsp;
    machine.wsp=machine.wsp+2*machine.ac[AA];
    if(machine.wsp>machine.wsl)
      throw std::runtime_error("Stack fault - upper limit - abort");
    if(machine.wsp<machine.wsb)
      throw std::runtime_error("Stack fault - lower limit - abort");
    if(machine.zero_claims)   // ruling 8: a positive delta is a claim
      zero_claim(machine, value, machine.wsp);
    return copy_segment(address, address+1);

   case WPSH:
    value=XX;
    while(true) {
      machine.wide_push(machine.ac[value]);
      if(value==AA) break;
      value=(value+1)%4;
    }
    return copy_segment(address, address+1);

   case WPOP:
    value=XX;
    while(true) {
      machine.ac[value]=machine.wide_pop();
      if(value==AA) break;
      value=(value+3)%4;
    }
    return copy_segment(address, address+1);

   case WFPSH: {
    int64_t bits;
    auto to_bits = [&](double d) -> int64_t { memcpy(&bits, &d, sizeof(bits)); return bits; };
    machine.quad_push(to_bits(machine.fplr));
    machine.quad_push(to_bits(machine.fpac[0]));
    machine.quad_push(to_bits(machine.fpac[1]));
    machine.quad_push(to_bits(machine.fpac[2]));
    machine.quad_push(to_bits(machine.fpac[3]));
    return copy_segment(address, address+1);
   }

   case WFPOP: {
    auto from_bits = [](int64_t b) -> double { double d; memcpy(&d, &b, sizeof(d)); return d; };
    machine.fpac[3]=from_bits(machine.quad_pop());
    machine.fpac[2]=from_bits(machine.quad_pop());
    machine.fpac[1]=from_bits(machine.quad_pop());
    machine.fpac[0]=from_bits(machine.quad_pop());
    machine.fplr=from_bits(machine.quad_pop());
    return copy_segment(address, address+1);
   }

   case XPEF:
    resolved=machine.eagle_x_resolve_indirect(copy_segment(address, address+1), AA);
    machine.wide_push(resolved);
    return copy_segment(address, address+2);

   case LPEF:
    resolved=machine.eagle_l_resolve_indirect(copy_segment(address, address+1), AA);
    machine.wide_push(resolved);
    return copy_segment(address, address+3);

   case XPEFB:
    resolved=machine.eagle_x_byte_indexed(copy_segment(address, address+1), AA);
    machine.wide_push(resolved);
    return copy_segment(address, address+2);

   case LPEFB:
    resolved=machine.eagle_l_byte_indexed(copy_segment(address, address+1), AA);
    machine.wide_push(resolved);
    return copy_segment(address, address+3);

   case XPSHJ:
    resolved=machine.eagle_x_resolve_indirect(copy_segment(address, address+1), AA);
    machine.wide_push(copy_segment(address, address+2));
    return resolved;

   case LPSHJ:
    resolved=machine.eagle_l_resolve_indirect(copy_segment(address, address+1), AA);
    machine.wide_push(copy_segment(address, address+3));
    return resolved;

   case WPOPJ:
    value=machine.wide_pop();
    return copy_segment(address, value);

   case DERR:
    machine.wide_push(address);
    machine.wide_push(((opcode>>10) & 0x1C) + ((opcode>>4) & 0x03));
    return copy_segment(address, machine.memory->read_word(copy_segment(address, 39)));
  }
  throw std::runtime_error("Internal error - some case is not returning");
}

} // namespace hw
