#include "OSContextTask.hpp"
#include "OSProcess.hpp"
#include "OSTask.hpp"
#include "OSError.hpp"
#include "../hw/Memory.hpp"
#include "../hw/Machine.hpp"
#include <cstdio>
#include <chrono>
#include <thread>
#include <stdexcept>
#include <mutex>
#include <condition_variable>




namespace os {
using namespace hw;

OSContextTask::OSContextTask(OSProcess* p, OSTask* t, Memory* m, Machine* mc)
  : OSContext(p, t, m, mc) {}

int32_t OSContextTask::dispatch_system_call(int32_t call) {
  switch(call) {
    case TASK:    return TASK_call();
    case REC:     return REC_call();
    case KILAD:   return KILAD_call();
    case DFRSCH:  return DFRSCH_call();
    case UIDSTAT: return UIDSTAT_call();
  }
  throw std::runtime_error("Dispatch system call - missing case");
}

int32_t OSContextTask::TASK_call() {
  int32_t packet_type = read_packet_wide("?DLNK");
  int32_t priority = read_packet_word("?DPRI");
  int32_t task_id = read_packet_word("?DID");
  int32_t start_address = read_packet_wide("?DPC");
  int32_t task_ac2 = read_packet_wide("?DAC2");
  int32_t stack_base = read_packet_wide("?DSTB");
  int32_t fault_handler = read_packet_word("?DSFLT");
  int32_t stack_high = read_packet_word("?DSSZ");
  int32_t stack_low = read_packet_word("?DSSL");
  int32_t flags = read_packet_word("?DFLGS");
  int32_t task_count = read_packet_word("?DNUM");

  printf("\nTASK:\n");
  printf("   packet type = %08X\n", packet_type);
  printf("   priority = %04X\n", priority);
  printf("   task ID = %04X\n", task_id);
  printf("   starting address = %08X\n", start_address);
  printf("   AC2 = %08X\n", task_ac2);
  printf("   stack base = %08X\n", stack_base);
  printf("   stack size high = %04X\n", stack_high);
  printf("   stack size low = %04X\n", stack_low);
  printf("   stack fault handler = %04X\n", fault_handler);
  printf("   flags = %04X\n", flags);
  printf("   task count = %04X\n\n", task_count);

  OSTask* new_task = new OSTask(process, start_address, stack_base,
    (stack_high << 16) + stack_low, fault_handler);
  if(process->register_task(new_task)) {
    new_task->machine->ac[2] = task_ac2;
    new_task->launch();
    write_packet_word("?DID", new_task->tid);
  }
  return SUCCESS;
}

int32_t OSContextTask::REC_call() {
  int32_t value;

  printf("\nREC:\n   mailbox = %08X\n", ac0);
  printf("   mailbox value = %08X\n\n", static_cast<int32_t>(memory->read_wide(ac0)));

  // Non-blocking outcomes are kept: a message already posted returns it,
  // and a single-task process falls through with ac1 = 0.
  //
  // The BLOCKING case is made fatal deliberately (Aug 2026). It would
  // wait for a wake via SYSCALL 0523, which is not implemented at all
  // (no constant, no case, absent from OSContext::context_for_call), so
  // the wait could never be satisfied — the original code polled in
  // 3-second sleeps forever. Worse, REC is classified LOCAL in
  // LockstepMediator, so master and clone would each block while holding
  // their lockstep slot and stall the pair gate. A hang gives us no
  // information; an abort names the situation. All four 0525/0523 sites
  // (I.LOCK 0x7017E7E3, I.UNLOCK 0x7017E800, MT?REC 0x7017E1B9,
  // MT?XMT 0x7017E19F) are on paths that have never executed.
  value = static_cast<int32_t>(memory->read_wide(ac0));
  if(value == 0 && process->count_tasks() > 1)
    throw std::runtime_error(
      "REC (0525) would block: inter-task wait is not supported - the "
      "wake path SYSCALL 0523 is unimplemented. Reached from I.LOCK "
      "(heap-lock contention) or MT?REC.");
  mem_write_wide(ac0, 0);

  printf("mailbox result: %08X\n", value);
  ac1 = value;
  return SUCCESS;
}

// ?DFRSCH (0550) - "Disables task rescheduling and indicates prior state
// of rescheduling." (AOS/VS)
//
//   Input   none
//   Output  AC0 = ?DSCH (0x8000) if task rescheduling was ALREADY
//                 disabled when ?DFRSCH was issued; AC1, AC2 undefined
//   No error codes are defined.
//
// Distinct from ?ERSCH (0526) and ?DRSCH (0527), which MT?ERSCH /
// MT?DRSCH use: ?DFRSCH also reports the prior state, so a called
// subroutine can restore it on exit.
//
// Reached only from SWAT.REX (0x7017E4BF, 0x7017E4E8), itself reachable
// only from ?FATAL. SWAT is Data General's symbolic debugger; the
// caller tests the result against ?DSCH exactly:
//
//   7017e4bf SYSCALL 0550
//   7017e4c3 WXORI 0,0x8000     ; was it already disabled?
//   7017e4c7 WBR   -> store 0 at 0x700001B2
//   7017e4c8 NLDAI 7,0          ; otherwise store 7
//
// The emulator runs tasks as real threads and does not gate scheduling
// on this flag - it is tracked only so the prior state is reported
// truthfully. Scheduling starts enabled, so the first call returns 0
// and SWAT.REX takes the "store 7" branch.
int32_t OSContextTask::DFRSCH_call() {
  bool was_disabled = process->rescheduling_disabled;
  process->rescheduling_disabled = true;

  printf("\n?DFRSCH:\n   rescheduling was %s\n\n",
         was_disabled ? "already disabled" : "enabled");

  ac0 = was_disabled ? static_cast<int32_t>(aos_symbol("?DSCH")) : 0;
  return SUCCESS;
}

int32_t OSContextTask::KILAD_call() {
  task->kill_address = ac0;
  return SUCCESS;
}

int32_t OSContextTask::UIDSTAT_call() {
  write_packet_word("?UUID", process->task_slot(task));
  write_packet_word("?UTSTAT", 0);
  write_packet_word("?UTID", task->tid);
  write_packet_word("?UTPRI", 0);
  return SUCCESS;
}

} // namespace os
