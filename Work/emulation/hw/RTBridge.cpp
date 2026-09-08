// src/hw/RTBridge.cpp
#include <set>
#include "RTBridge.hpp"
#include "Machine.hpp"
#include "Lockstep.hpp"
#include "../debug/CallStack.hpp"
#include "../os/OSContext.hpp"
#include "../os/OSTask.hpp"
#include "../os/OSProcess.hpp"
#include "../os/OSError.hpp"
#include "../os/LockstepMediator.hpp"
#include "../os/Trace.hpp"
#include <cstdio>
#include <memory>

namespace hw {

RTBridge::RTBridge(Machine& machine, Convention convention)
  : machine(machine)
{
  return_addr=static_cast<uint32_t>(machine.ac[3]);
  saved_wsp=machine.wsp;
  saved_psr=machine.get_psr();
  frame_word=0;
  argc=0;
  if(convention==LCALL_FRAME) {
    frame_word=static_cast<uint32_t>(machine.memory->read_wide(static_cast<uint32_t>(machine.wsp)));
    argc=static_cast<int32_t>(frame_word & 0x7FFF);
  }
  for(int i=0; i<3; i++)
    saved_ac[i]=machine.ac[i];
  saved_c=machine.c;
}

uint32_t RTBridge::emulate_frame_ss() {
  // Exact WSSVS/WSSVR image: pushes psr<<16 FIRST, then ac0, ac1, ac2,
  // wfp, ac3|carry<<31; ac3=wfp=wsp after the six pushes. Written as
  // residue without moving wsp.
  uint32_t base=static_cast<uint32_t>(machine.wsp);
  machine.memory->write_wide(base+2, saved_psr<<16);
  machine.memory->write_wide(base+4, saved_ac[0]);
  machine.memory->write_wide(base+6, saved_ac[1]);
  machine.memory->write_wide(base+8, saved_ac[2]);
  machine.memory->write_wide(base+10, machine.wfp);
  machine.memory->write_wide(base+12, static_cast<int32_t>(return_addr) | (saved_c<<31));
  zero_frame_claim(base+12);
  return base+12;   // the would-be frame pointer (ac3 after WSSVS/WSSVR)
}

// Ruling 8 clone-side symmetry (docs/Layering.md ruling 8;
// docs/Project10). The MASTER emulates this routine, so its
// WSAVx/WSSVx now ZEROES the claimed frame area; this native path
// replaces that instruction, so it must zero the same words — or any
// read-before-write local reads master=0 / clone=stale, the B1 class
// re-created one layer down. frame_size comes from the entry
// instruction itself: dispatch runs the wrapper in place of
// fetch+decode at the entry, so machine.pc IS the entry pc and the
// operand word sits at pc+1. The raw opcode is verified first
// (WSAVR/WSAVS/WSSVR/WSSVS are fixed 16-bit encodings) so a
// non-frame-shaped entry can never zero a garbage-sized range.
// Interior frames written manually as wrapper residue are NOT covered
// here — see docs/Project10/REPORT.md "Residuals".
void RTBridge::zero_frame_claim(uint32_t fp) {
  if(!machine.zero_claims) return;
  uint32_t entry=static_cast<uint32_t>(machine.pc);
  uint32_t op=machine.memory->read_instruction_word(entry) & 0xFFFF;
  if(op!=0xA729 && op!=0xA739 && op!=0x8729 && op!=0x8739) {
    // Dedup (user, Aug 14): trampoline/alias entries (first opcode a
    // jump — A6C9/A6E9 at the ?LIB_ERROR / ?ERMSG-region aliases)
    // legitimately have no claim at the entry; all sessions green.
    // Announce each entry ONCE per run; genuinely new shapes stay loud.
    static std::set<uint32_t> seen_odd_entries;
    if(seen_odd_entries.insert(entry).second)
      fprintf(stderr, "RTBridge::zero_frame_claim: entry %08X opcode %04X is not"
                      " WSAVx/WSSVx — claim NOT zeroed (new entry shape?"
                      " — further occurrences silenced)\n", entry, op);
    return;
  }
  uint32_t fs=machine.memory->read_word(Machine::copy_segment(entry, entry+1)) & 0xFFFF;
  for(uint32_t w=fp+2; w<fp+2+fs*2; w++)
    machine.memory->write_word(Machine::copy_segment(entry, w), 0);
}

uint32_t RTBridge::native_return_ss(int32_t final_wsp) {
  machine.set_psr(saved_psr);
  machine.wsp=final_wsp;
  for(int i=0; i<3; i++)
    machine.ac[i]=saved_ac[i];
  machine.c=saved_c;
  machine.ac[3]=machine.wfp;   // wfp is unchanged (normal) or already restored by the wrapper
  // NO shadow call-stack pop: for LJSR routines the shadow frame is
  // pushed by WSSVS/WSSVR itself (EagleStack caller-pattern detection),
  // which never runs on the native path — there is nothing to pop.
  machine.native_break=true;
  return return_addr;
}

RTBridge::RTBridge(Machine& machine)
  : machine(machine)
{
  return_addr=static_cast<uint32_t>(machine.ac[3]);
  saved_wsp=machine.wsp;
  saved_psr=machine.get_psr();
  frame_word=static_cast<uint32_t>(machine.memory->read_wide(static_cast<uint32_t>(machine.wsp)));
  argc=static_cast<int32_t>(frame_word & 0x7FFF);
  for(int i=0; i<3; i++)
    saved_ac[i]=machine.ac[i];
  saved_c=machine.c;
}

uint32_t RTBridge::arg_pointer(int n) const {
  uint32_t slot=static_cast<uint32_t>(machine.wsp-2*n);
  return static_cast<uint32_t>(machine.memory->read_wide(slot));
}

int32_t RTBridge::arg_word(int n) const {
  int32_t value=static_cast<int32_t>(machine.memory->read_word(arg_pointer(n)));
  return static_cast<int32_t>(value<<16)>>16;
}

int32_t RTBridge::arg_wide(int n) const {
  return machine.memory->read_wide(arg_pointer(n));
}

void RTBridge::set_arg_word(int n, int32_t value) {
  machine.memory->write_word(arg_pointer(n), static_cast<uint32_t>(value)&0xFFFF);
}

void RTBridge::set_arg_wide(int n, int32_t value) {
  machine.memory->write_wide(arg_pointer(n), value);
}

void RTBridge::set_return_ac(int n, int32_t value) {
  saved_ac[n]=value;
}

uint32_t RTBridge::emulate_frame() {
  // Exact WSAVS image: pushes ac0, ac1, ac2, wfp, ac3|carry<<31 (in that
  // order, so ac0 lands lowest), then ac3=wfp=wsp+10. We write the words
  // without moving wsp — the frame is residue, not live state.
  uint32_t base=static_cast<uint32_t>(machine.wsp);
  machine.memory->write_wide(base+2, saved_ac[0]);
  machine.memory->write_wide(base+4, saved_ac[1]);
  machine.memory->write_wide(base+6, saved_ac[2]);
  machine.memory->write_wide(base+8, machine.wfp);
  machine.memory->write_wide(base+10, static_cast<int32_t>(return_addr) | (saved_c<<31));
  zero_frame_claim(base+10);
  return base+10;   // the would-be frame pointer (ac3 after WSAVS)
}

void RTBridge::write_frame_word(uint32_t frame_base, int32_t offset, int32_t value) {
  machine.memory->write_word(frame_base+static_cast<uint32_t>(offset), static_cast<uint32_t>(value)&0xFFFF);
}

void RTBridge::write_frame_wide(uint32_t frame_base, int32_t offset, int32_t value) {
  machine.memory->write_wide(frame_base+static_cast<uint32_t>(offset), value);
}

void RTBridge::write_frame_byte(uint32_t frame_base, int32_t byte_offset, int32_t value) {
  machine.memory->write_byte(frame_base*2+static_cast<uint32_t>(byte_offset), static_cast<uint32_t>(value)&0xFF);
}

uint32_t RTBridge::native_return() {
  machine.set_psr(static_cast<int32_t>(frame_word>>16));
  machine.wsp=machine.wsp-2-2*argc;   // pop frame wide + args (callee-pops)
  for(int i=0; i<3; i++)
    machine.ac[i]=saved_ac[i];
  machine.c=saved_c;
  machine.ac[3]=machine.wfp;
  machine.call_stack->native_return(static_cast<int32_t>(return_addr));   // pop the shadow frame the LCALL pushed
  machine.native_break=true;   // pair rendezvous at the post-call point
  return return_addr;
}

namespace {
// Clears Lockstep::task_thread_label on every exit path. The emulated
// dispatch clears it only on the normal path (a throw there aborts the
// machine anyway); clearing on throw here is host-diagnostic hygiene,
// not emulated-visible state.
struct ThreadLabelGuard {
  ThreadLabelGuard(const char* label) { Lockstep::task_thread_label=label; }
  ~ThreadLabelGuard() { Lockstep::task_thread_label=nullptr; }
};
}

int32_t RTBridge::syscall(int32_t call, uint32_t entry_pc,
                          int32_t& ac0, int32_t& ac1, int32_t& ac2) {
  os::OSTask* task=machine.task;
  os::OSProcess* process=task->process;

  // Step 2 of the emulated dispatch: context + AC copy-in.
  // unique_ptr because dispatch can throw (unimplemented call, REC abort,
  // anything a handler raises) — a raw delete would leak on that path.
  std::unique_ptr<os::OSContext> context(
    os::OSContext::context_for_call(call, process, task, machine.memory, &machine));
  context->ac0=ac0;
  context->ac1=ac1;
  context->ac2=ac2;

  // Logging parity with the emulated path (minus the dispatchSystemCall/
  // <wsp> line): same lines, native entry address in place of the trap
  // address, so an scalls diff of master vs clone reads call-for-call.
  if(process->system_call_logging) {
    printf("System Call %o, called from %08X\n", call, entry_pc);
    printf(" ac0=%08X\n", ac0);
    printf(" ac1=%08X\n", ac1);
    printf(" ac2=%08X\n", ac2);
  }
  if(os::Trace::enabled("scalls")) {
    char buf[128];
    snprintf(buf, sizeof(buf), "tid=%d call=%s ac0=%08X ac1=%08X ac2=%08X",
             task->tid, os::Trace::call_name(call).c_str(), ac0, ac1, ac2);
    os::Trace::line("scalls", process->instance_label, buf);
  }

  // Step 3: mediator-routed dispatch. Bypassing the mediator would punch
  // a hole in the verification this project runs on.
  int32_t error;
  {
    ThreadLabelGuard guard(process->instance_label.c_str());
    if(os::LockstepMediator::applies(task))
      error=os::LockstepMediator::dispatch(task, call, context.get(),
                                           entry_pc, /*site_native=*/true);
    else
      error=context->dispatch_system_call(call);
  }

  if(os::Trace::enabled("scalls")) {
    char buf[128];
    snprintf(buf, sizeof(buf), "tid=%d call=%s ret err=%04X ac0=%08X ac1=%08X",
             task->tid, os::Trace::call_name(call).c_str(), error,
             context->ac0, context->ac1);
    os::Trace::line("scalls", process->instance_label, buf);
  }
  if(process->system_call_logging)
    printf("RETURNING AC1=%08X\n", context->ac1);

  if(error==os::OSError::SUCCESS) {
    ac0=context->ac0;
    ac1=context->ac1;
    ac2=context->ac2;
  }
  else
    printf("****** ERROR RETURN ******  CODE=%04X\n\n", error);   // unconditional in the emulated path too
  return error;
}

uint32_t RTBridge::native_transfer(Machine& machine, uint32_t target_pc) {
  machine.native_break=true;
  return target_pc;
}

} // namespace hw
