#include "Machine.hpp"
#include "IRExec.hpp"
#include "AddressBook.hpp"
#include "../os/OSProcess.hpp"
#include "../os/Trace.hpp"
#include "Lockstep.hpp"
#include "RTStubs.hpp"
#include "BlockSync.hpp"
#include "MachineThread.hpp"
#include "Decoder.hpp"
#include "Instruction.hpp"
#include "../debug/Capture.hpp"
#include "../debug/CallStack.hpp"
#include "../debug/SymbolTable.hpp"
#include "../types/Context.hpp"
#include "../types/OperatingSystem.hpp"
#include <cstdio>
#include <stdexcept>
#include <cstring>




namespace hw {
using namespace debug;

MachineThread* Machine::machine_thread = nullptr;
int32_t Machine::zero_mode = Machine::ZERO_BOTH;   // -zero= default (ruling 8)

Machine::Machine(OSProcess* process, OSTask* task, SymbolTable* symbols, Memory* memory)
  : debug(false), process(process), task(task), symbols(symbols), memory(memory),
    call_stack(nullptr),
    pc(0), fplr(0.0), c(0),
    ovk(0), ovr(0), ires(0), ixct(0), ffp(0), sr(0), fpr(0),
    wsb(0), wsl(0), wsp(0), wfp(0), instruction_count(0), block_ordinal(0),
    halt_ptr(nullptr),
    lockstep_role(0), lockstep_ordinal(-1), rtcov(RTStubs::coverage_for(process)),
    native_break(false), native_span(false), rt_pending_return(0),
    pending_native(nullptr), terminal_reached(false),
    zero_claims(zero_mode == ZERO_BOTH), args_written(false)
{
  call_stack = new CallStack(symbols, this);
  for(int i=0; i<8; i++)
    segments[i] = new Segment(i < 4);
  for(int i=0; i<4; i++) { ac[i]=0; fpac[i]=0.0; quads[i]=0; }
}

Machine::~Machine() {
  delete call_stack;
  for(int i=0; i<8; i++)
    delete segments[i];
}

void Machine::copy_state(Machine& current) {
  pc = current.pc;
  for(int i=0; i<4; i++) {
    ac[i] = current.ac[i];
    fpac[i] = current.fpac[i];
  }
  fplr = current.fplr;
  c = current.c;
  ovr = current.ovr;
  wfp = current.wfp;
  wsp = current.wsp;
  wsb = current.wsb;
  wsl = current.wsl;
}

void Machine::add_debug(const std::string& name) {
  call_stack->debug.insert(name);
}

int32_t Machine::get_psr() {
  return (ovk<<15)+(ovr<<14)+(ires<<13)+(ixct<<12)+(ffp<<11)+sr;
}

void Machine::set_psr(int32_t psr) {
  sr = psr & 0x03;
  psr = psr >> 11;
  ffp = psr & 0x01; psr >>= 1;
  ixct = psr & 0x01; psr >>= 1;
  ires = psr & 0x01; psr >>= 1;
  ovr = psr & 0x01; psr >>= 1;
  ovk = psr & 0x01;
}

void Machine::wide_push(int32_t wide) {
  wsp = wsp + 2;
  if(wsp > wsl && (wsl & static_cast<int32_t>(0x80000000)) == 0) {
    char buf[64];
    snprintf(buf, sizeof(buf), "Stack fault - upper limit - abort, pc=%08X", pc);
    throw std::runtime_error(buf);
  }
  memory->write_wide(copy_segment(pc, wsp), wide);
}

int32_t Machine::wide_pop() {
  int32_t stack_pointer = copy_segment(pc, wsp);
  int32_t value = memory->read_wide(stack_pointer);
  wsp = wsp - 2;
  if(stack_pointer < wsb) {
    char buf[64];
    snprintf(buf, sizeof(buf), "Stack fault - lower limit - abort, pc=%08X", pc);
    throw std::runtime_error(buf);
  }
  return value;
}

void Machine::quad_push(int64_t quad) {
  wide_push(static_cast<int32_t>(static_cast<uint64_t>(quad) >> 32));
  wide_push(static_cast<int32_t>(quad & 0xFFFFFFFF));
}

int64_t Machine::quad_pop() {
  int64_t low = wide_pop();
  int64_t high = wide_pop();
  return (high << 32) | (low & 0xFFFFFFFF);
}

uint32_t Machine::eagle_x_byte_indexed(uint32_t pc_val, uint32_t ii) {
  int32_t address = memory->read_word(pc_val);
  // Sign-extend 16-bit value
  int32_t signed_addr = (address << 16) >> 16;

  switch(ii) {
    case 0:
      address = set_byte_segment(get_segment(pc_val), address);
      break;
    case 1:
      address = pc_val * 2 + signed_addr;
      break;
    case 2:
      address = ac[2] * 2 + signed_addr;
      address = set_byte_segment(get_segment(pc_val), address);
      break;
    case 3:
      address = ac[3] * 2 + signed_addr;
      address = set_byte_segment(get_segment(pc_val), address);
      break;
  }
  return address;
}

uint32_t Machine::eagle_l_byte_indexed(uint32_t pc_val, uint32_t ii) {
  int32_t address = memory->read_wide(pc_val);

  switch(ii) {
    case 1:
      address = pc_val * 2 + address;
      break;
    case 2:
      address = ac[2] * 2 + address;
      break;
    case 3:
      address = ac[3] * 2 + address;
      break;
  }
  return address;
}

uint32_t Machine::eagle_x_resolve_indirect(uint32_t pc_val, uint32_t ii) {
  int32_t count = 0;
  int32_t address = memory->read_word(pc_val);
  int32_t indirect = (address & 0x8000) << 16;
  address = address & 0x7FFF;

  switch(ii) {
    case 0:
      address = copy_segment(pc_val, address);
      break;
    case 1:
      address = pc_val + ((address << 17) >> 17);
      break;
    case 2:
      address = ac[2] + ((address << 17) >> 17);
      address = copy_segment(pc_val, address);
      break;
    case 3:
      address = ac[3] + ((address << 17) >> 17);
      address = copy_segment(pc_val, address);
      break;
  }
  address = (address & 0x7FFFFFFF) | indirect;
  while(address < 0) {
    if(count++ == 15)
      throw std::runtime_error("Indirection limit reached");
    address = memory->read_wide(address & 0x7FFFFFFF);
  }
  return address;
}

uint32_t Machine::eagle_l_resolve_indirect(uint32_t pc_val, uint32_t ii) {
  int32_t count = 0;
  int32_t address = memory->read_wide(pc_val);
  int32_t indirect = address & static_cast<int32_t>(0x80000000);
  address = address & 0x7FFFFFFF;

  switch(ii) {
    case 0:
      break;
    case 1:
      address = pc_val + address;
      break;
    case 2:
      address = ac[2] + address;
      break;
    case 3:
      address = ac[3] + address;
      break;
  }
  address = (address & 0x7FFFFFFF) | indirect;
  while(address < 0) {
    if(count++ == 15)
      throw std::runtime_error("Indirection limit reached");
    address = memory->read_wide(address & 0x7FFFFFFF);
  }
  return address;
}

uint32_t Machine::eagle_resolve_indirect(uint32_t address) {
  int32_t addr = static_cast<int32_t>(address);
  int32_t count = 0;
  while(addr < 0) {
    if(count++ == 15)
      throw std::runtime_error("Indirection limit reached");
    addr = memory->read_wide(addr & 0x7FFFFFFF);
  }
  return addr;
}

uint32_t Machine::run_steps(uint32_t address, int32_t count) {
  Instruction* instruction;
  uint32_t opcode, new_pc;
  // RT-entry sync: under lockstep, client batches break when pc arrives at
  // a runtime entry so master and clone pair at every runtime call with
  // argument state compared. Permanent architecture — the sync identity is
  // the entry address as a logical event, valid before and after the
  // routine is translated (see RTStubs.hpp).
  bool rt_sync = rtcov != nullptr && Lockstep::enabled &&
    (lockstep_role == Lockstep::MASTER || lockstep_role == Lockstep::CLONE);
  int32_t pending_guard = 0;
  // Gen-6.0 heartbeat (docs/Project22/BlockSyncDesign.md): listed block
  // entries since this batch began. Every batch end is a rendezvous, so
  // "K entries since the last rendezvous" is exactly "K entries this
  // batch"; the counter is batch-local by construction.
  uint32_t blocks_since_sync = 0;

  pc = address;
  while(count > 0) {
    if(halt_ptr && *halt_ptr) break;
    if(pending_native) {
      // Crossings-only checker: a deferred L1→L2 dispatch (armed at the
      // LCALL/XCALL/LJSR site or by inject_fire). The previous batch
      // ended AT the entry pc — the crossing rendezvous, argument state
      // verified — and this resume runs the native implementation in
      // place of fetch+decode at the entry. Not counted as an
      // instruction: the native body was zero instructions when it ran
      // inside the dispatch instruction, and stays zero here.
      uint32_t (*fn)(Machine&) = pending_native;
      pending_native = nullptr;
      new_pc = fn(*this);
    } else {
    count--;
    // P23 (Gen-6.1) IR dispatch: a block PRESENT in quest.ir runs as IR
    // on the CLONE; the master always emulates (docs/Project23/
    // IRPhase1.md §1). Placed at the top of the fetch path so ordinal
    // counting, break decisions, and every rendezvous below see the
    // block exit exactly as they would an instruction result.
    if(IRExec::instance && lockstep_role == Lockstep::CLONE &&
       IRExec::instance->has(static_cast<uint32_t>(pc))) {
      new_pc = IRExec::instance->run_block(*this, static_cast<uint32_t>(pc));
      if(new_pc == 0x30000000)
        return new_pc;
    } else {
    if(rtcov && static_cast<uint32_t>(pc) >= RTStubs::start &&
       static_cast<uint32_t>(pc) < RTStubs::stop)
      rtcov[static_cast<uint32_t>(pc) - RTStubs::start] = 1;
    if(rtcov)
      debug::Capture::check(*this);   // env-gated derivation captures (QUEST_CAPTURE)
    opcode = memory->read_instruction_word(pc);
    instruction = Decoder::decode(segments[(address >> 28) & 0x07]->lef, opcode);
    if(!instruction) {
      char buf[64];
      snprintf(buf, sizeof(buf), "Opcode %04X has not been defined", opcode);
      throw std::runtime_error(buf);
    }
    new_pc = instruction->execute(*this, pc, opcode);
    if(new_pc == 0x30000000)
      return new_pc;
    if(ovk > 0 && ovr > 0) {
      char buf[64];
      snprintf(buf, sizeof(buf), "Overflow occurred at %08X", pc);
      throw std::runtime_error(buf);
    }
    instruction_count++;
    }
    }
    pc = new_pc;
    // Gen-6 block-entry counting (docs/Project22/BlockSyncDesign.md): every
    // arrival-transition at a LISTED game block entry ticks the ordinal —
    // BEFORE any break decision below, so master and clone count
    // identically whichever rendezvous (terminal, crossing, span exit,
    // heartbeat) ends the batch here. A batch's initial pc is not counted:
    // at a break it was counted when arrived at; a syscall-return resume
    // enters through OS code both engines traverse identically.
    if(rt_sync && BlockSync::listed(static_cast<uint32_t>(pc))) {
      block_ordinal++;
      blocks_since_sync++;
    }
    // Detached-master tripwire (user ruling, Aug 29 2026): the MASTER
    // arriving at the per-turn command dispatch (START_TURN) with its
    // clone detached means the player is about to keep playing an
    // UNVERIFIED master on the strength of one stderr line. Hard abort
    // instead, write-back suppressed — post-detach master state is
    // deliberately not trusted with the data files ("it shouldn't
    // happen"). Graceful shutdowns can't false-fire: START_TURN's ONLY
    // caller in the listing is the main-loop site 7015C5DC, and the
    // detach-kind terminals (I.STOP/?FATAL) never return to the loop —
    // the master runs them forward to exit. ?RETURN retirement marks
    // detached at the exit syscall itself, after the last game
    // instruction. Empirical: a full live-clone turn does not fire
    // (clean leg); a forced pre-turn detach fires at the first
    // START_TURN arrival (QUEST_TERMINAL leg).
    if(rt_sync && lockstep_role == Lockstep::MASTER &&
       RTStubs::turn_loop_pc != 0 &&
       static_cast<uint32_t>(pc) == RTStubs::turn_loop_pc &&
       Lockstep::is_detached(lockstep_ordinal)) {
      char buf[200];
      snprintf(buf, sizeof(buf),
               "Internal error: Master did not terminate after clone detach "
               "(ordinal %d, turn dispatch START_TURN %08X)",
               lockstep_ordinal, RTStubs::turn_loop_pc);
      Lockstep::abort_world(buf, this, /*save=*/false);
      throw std::runtime_error(buf);
    }
    // Terminal detach (Lockstep::detach): pc arriving at a terminal point
    // ends the batch with the terminal flag set. Both engines emulate the
    // same code, so master and clone converge here and form one final
    // verified pair; compare_pair then detaches (or aborts) the clone.
    // Covers the QUEST_TERMINAL test pc AND the permanent game-range
    // terminal sites (RTStubs::game_terminals — e.g. the direct
    // SYSCALL 0310 forced-exit site at 0x7017700F), any address.
    // Generation 3 (docs/CheckerHistory.md): the terminal flag is set on
    // ARRIVAL, however control got here. A native wrapper's transfer
    // landing on a terminal pc is a terminal arrival like any other —
    // consume the break here and mark the batch as a native span, so the
    // terminal pair carries the same flags an exit pair would.
    if(rt_sync &&
       (static_cast<uint32_t>(pc) < RTStubs::start ||
        static_cast<uint32_t>(pc) >= RTStubs::stop) &&
       RTStubs::is_terminal_pc(static_cast<uint32_t>(pc))) {
      rt_pending_return = 0;
      terminal_reached = true;
      if(native_break) {
        native_break = false;
        native_span = true;
      }
      return pc;
    }
    // Crossings-only checker: the L1→L2 RETURN crossing — L1 code
    // arriving in the signal tail by RETURNING into it (handler WRTN to
    // DISPATCH_RET / E3EF; L2Contract.md §5). A rendezvous only at
    // depth 0: inside a pending span both engines absorb it (the clone's
    // fallback and the master's run-to-return pass through these pcs as
    // interior), and a native_break already breaks on its own terms.
    if(rt_sync && rt_pending_return == 0 && !native_break &&
       RTStubs::is_return_crossing(static_cast<uint32_t>(pc)))
      return pc;
    // Fault injector (Project 5): at the armed site, synthesize an
    // O?SIGNAL raise exactly as a real call site would — identical
    // staging on both roles; the clone's registry (alone non-empty)
    // dispatches native, the master emulates from the entry via the
    // rt_sync entry block below, exactly like a real LCALL. One shot.
    // Register poke (Project 27): at the armed pc, on arrival, set one
    // AC on both roles before anything at pc runs (the clone's IR block
    // for pc has not started; it reads machine.ac on entry). One shot.
    if(process->poke_armed &&
       static_cast<uint32_t>(pc) == RTStubs::poke_pc) {
      process->poke_armed = false;
      fprintf(stderr, "RTStubs: POKE firing at %08X: ac%d %08X -> %08X\n",
              static_cast<uint32_t>(pc), RTStubs::poke_ac,
              static_cast<uint32_t>(ac[RTStubs::poke_ac]), static_cast<uint32_t>(RTStubs::poke_value));
      ac[RTStubs::poke_ac] = RTStubs::poke_value;
    }
    if(process->inject_armed &&
       static_cast<uint32_t>(pc) == RTStubs::inject_site) {
      process->inject_armed = false;
      pc = static_cast<int32_t>(RTStubs::inject_fire(*this));
    }
    if(native_break) {
      native_break = false;
      if(rt_pending_return == 0) {
        // Clone: a native runtime call just completed inside this
        // instruction; rendezvous at the post-call point.
        // Generation 3 (docs/CheckerHistory.md): if that point is a
        // terminal (a native_transfer to the ?FATAL door), this is a
        // terminal ARRIVAL — same rendezvous, plus the terminal flag.
        // The batch keeps native_span, so the terminal pair carries the
        // exit-pair flags (count exemption; pc + registers compared).
        native_span = true;
        if(RTStubs::is_terminal_pc(static_cast<uint32_t>(pc)))
          terminal_reached = true;
        return pc;
      }
      // Clone, inside a native-FALLBACK span (rt_pending_return set by
      // the fallback path): an inner translated routine (e.g. I.LOCK
      // inside emulated I.FREEW) just ran natively. Swallow its break —
      // the master's run-to-return ignores the inner entry too, so the
      // pair must rendezvous only at the outer return.
    }
    if(rt_pending_return != 0) {
      // Master: inside the emulated body of a routine the clone runs
      // natively. Run to the matching return; ignore batch exhaustion and
      // entry breaks until there (inner RT entries are absorbed).
      // Exception 1: a body that dies lands on a terminal entry and never
      // reaches the return — escape there instead of spinning to the
      // runaway guard.
      // Exception 2 (transfer pairing, see docs/SharedProtocol.md): a
      // body that exits NON-LOCALLY — handler dispatch, unwind resume,
      // or an LJSR continuation — leaves the RT range at a pc the
      // clone's native wrapper also ends at (native_transfer). RT code
      // only ever reaches game-range pcs at returns, dispatches, and
      // resumes, so "pc left the RT range" is a complete and safe
      // terminator; it also closes the old runaway-guard gap for any
      // translated routine whose emulated body signals.
      if(static_cast<uint32_t>(pc) >= RTStubs::start &&
         static_cast<uint32_t>(pc) < RTStubs::stop &&
         RTStubs::terminal_bits[static_cast<uint32_t>(pc) - RTStubs::start]) {
        rt_pending_return = 0;
        terminal_reached = true;
        return pc;
      }
      if(static_cast<uint32_t>(pc) == rt_pending_return ||
         static_cast<uint32_t>(pc) < RTStubs::start ||
         static_cast<uint32_t>(pc) >= RTStubs::stop) {
        rt_pending_return = 0;
        native_span = true;
        return pc;
      }
      if(count <= 0) {
        count = 1;
        if(++pending_guard > 10000000)
          throw std::runtime_error("run-to-return: neither return nor RT-range exit reached (should be impossible; see SharedProtocol.md)");
      }
      continue;
    }
    // Entry block, keyed on LAYER TRANSITIONS (crossings-only checker,
    // user-ratified Aug 2026; docs/CrossingsChecker.md). The sync
    // surface: the L1 fabric pairs as it always has (L0/L1 entries,
    // syscalls, the batch heartbeat); L1↔L2 crossings pair in BOTH
    // directions; interior L2 is invisible; L3 pairs once at the door
    // (terminal machinery). "Check the game's fabric continuously,
    // check the handler machinery at its skin, check death at the door."
    if(rt_sync && static_cast<uint32_t>(pc) >= RTStubs::start &&
       static_cast<uint32_t>(pc) < RTStubs::stop &&
       RTStubs::entry_bits[static_cast<uint32_t>(pc) - RTStubs::start]) {
      if(RTStubs::terminal_bits[static_cast<uint32_t>(pc) - RTStubs::start]) {
        terminal_reached = true;
        return pc;
      }
      bool l2 = RTStubs::l2_bits[static_cast<uint32_t>(pc) - RTStubs::start] != 0;
      if(RTStubs::translated_bits[static_cast<uint32_t>(pc) - RTStubs::start]) {
        if(pending_native)
          // Clone at an L1→L2 crossing with dispatch deferred by the
          // call site: break AT the entry — the crossing rendezvous.
          // The native implementation runs on resume (loop top).
          return pc;
        // Master entering the emulated body (the clone dispatches
        // natively and only lands here via the deferral above). ac3
        // holds the return address (set by LCALL/XCALL/LJSR). Absorb
        // the subtree to the L2→L1 exit; for an L2 crossing, pair at
        // the entry first — mirroring the clone's deferred break.
        rt_pending_return = static_cast<uint32_t>(ac[3]);
        if(l2)
          return pc;
        continue;
      }
      if(l2) {
        // Untranslated L2 entry reached from L1 (frozen/dead paths;
        // never observed live). Pair at the entry, then arm the
        // pending span on BOTH roles so the whole emulated L2 subtree
        // is interior — no rendezvous until the L2→L1 exit (return pc
        // or RT-range departure), exactly like a fallback span. The
        // four dispatch-site guards keep inner native calls suppressed
        // symmetrically for the span's duration.
        rt_pending_return = static_cast<uint32_t>(ac[3]);
      }
      return pc;
    }
    // Gen-6.0 heartbeat: the K-th listed block entry since the last
    // rendezvous ends the batch AT the entry (pre-execution), replacing
    // the 500-instruction batch as the sync fabric's clock. Placed after
    // every gate check above so gates keep precedence — a gate arrival
    // that is also the K-th entry pairs as the gate it is.
    if(rt_sync && blocks_since_sync >= BlockSync::sync_k)
      return pc;
  }
  // Loop exit: a halted task returns normally; a lockstep client
  // exhausting its instruction budget did NOT reach any rendezvous in
  // RUNAWAY_GUARD instructions — that is the runaway, and it fails loud
  // (METHOD §8) instead of quietly pairing on an instruction count the
  // Gen-6 surface no longer speaks.
  if(rt_sync && count <= 0) {
    char buf[112];
    snprintf(buf, sizeof(buf),
             "block-sync: no rendezvous within %d instructions (runaway guard), pc=%08X",
             BlockSync::RUNAWAY_GUARD, pc);
    throw std::runtime_error(buf);
  }
  return pc;
}

uint32_t Machine::run(uint32_t address) {
  uint32_t new_pc;

  pc = address;
  while(!(halt_ptr && *halt_ptr)) {
    // Gen-6.0 (docs/Project22/BlockSyncDesign.md): lockstep QUEST clients
    // sync on the K-block heartbeat inside run_steps; their instruction
    // budget is only the runaway guard, whose exhaustion THROWS. The
    // server keeps plain instruction batches (it is not paired). A
    // non-QUEST program duplicated under -lockstep (no RT range, no
    // blocks file — a generic capability never used for Quest work)
    // retains the historical 500-instruction pairing.
    int32_t batch = 1000;
    if(Lockstep::enabled &&
       (lockstep_role == Lockstep::MASTER || lockstep_role == Lockstep::CLONE))
      batch = rtcov ? BlockSync::RUNAWAY_GUARD : 500;
    new_pc = machine_thread->run_steps(this, pc, batch);
    if(new_pc == 0x30000000)
      return new_pc;   // System call - caller handles dispatch
    pc = new_pc;
  }
  return pc;
}

void Machine::dump_stack_area(uint32_t address, int32_t size) {
  printf("Stack area: %08X called from %08X\n", address, pc);
  for(int32_t i = 0; i < size; i++)
    printf("   %08X %02X = %04X\n", address + i, i, memory->read_word(address + i));
}

void Machine::backtrace() {
  call_stack->backtrace(symbols, pc);
}

} // namespace hw
