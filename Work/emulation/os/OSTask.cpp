#include "OSTask.hpp"
#include "LockstepMediator.hpp"
#include "../hw/Lockstep.hpp"
#include "Trace.hpp"
#include "OSProcess.hpp"
#include "OS.hpp"
#include "OSError.hpp"
#include "OSContext.hpp"
#include "../hw/Machine.hpp"
#include "../hw/Memory.hpp"
#include "../debug/CallStack.hpp"
#include <cstdio>
#include <stdexcept>




namespace os {
using namespace debug;
using namespace hw;

thread_local Machine* OSTask::machine_for_backtrace = nullptr;

void OSTask::backtrace() {
  if(machine_for_backtrace) {
    machine_for_backtrace->backtrace();
  }
}

OSTask::OSTask(OSProcess* process, int32_t start_address,
               int32_t wfp, int32_t wsp, int32_t wsb, int32_t wsl,
               int32_t stack_fault_handler)
  : process(process), tid(-1), working_directory(process->working_directory),
    start_address(start_address), wfp(wfp), wsp(wsp), wsb(wsb), wsl(wsl),
    stack_fault_handler(stack_fault_handler), kill_address(-1),
    memory(process->memory), machine(nullptr), halt(false)
{
  machine = new Machine(process, this, process->symbols, memory);
  machine->wfp = wfp;
  machine->wsp = wsp;
  machine->wsb = wsb;
  machine->wsl = wsl;
  machine->halt_ptr = &halt;
  machine->mapper.configure(machine, process->mapper_book, /*is_main_task=*/true);
  machine->call_stack->call(start_address, -1, -1, 0);
}

OSTask::OSTask(OSProcess* process, int32_t start_address,
               int32_t stack_base, int32_t stack_size,
               int32_t stack_fault_handler)
  : process(process), tid(-1), working_directory(process->working_directory),
    start_address(start_address), wfp(0), wsp(stack_base), wsb(stack_base),
    wsl(stack_base + stack_size), stack_fault_handler(stack_fault_handler),
    kill_address(-1), memory(process->memory), machine(nullptr), halt(false)
{
  machine = new Machine(process, this, process->symbols, memory);
  machine->wfp = 0;
  machine->wsp = stack_base;
  machine->wsb = stack_base;
  machine->wsl = stack_base + stack_size;
  machine->halt_ptr = &halt;
  machine->mapper.configure(machine, process->mapper_book, /*is_main_task=*/false);
  machine->call_stack->call(start_address, -1, -1, 0);
}

OSTask::~OSTask() {
  if(thread.joinable())
    thread.detach();
  delete machine;
}

std::string OSTask::full_path(const std::string& filename) {
  if(!filename.empty() && filename[0] == ':') return filename;
  if(!filename.empty() && filename[0] == '@') return filename;
  if(working_directory == ":")
    return ":" + filename;
  return working_directory + ":" + filename;
}

void OSTask::launch() {
  thread = std::thread([this]{ run(); });
}

void OSTask::halt_task() {
  halt = true;
  // Interrupt the process's message queue so tasks blocked in IREC wake up
  OS::global.interrupt_queue(process->pid);
}

void OSTask::run() {
  machine_for_backtrace = machine;
  try {
    uint32_t pc = static_cast<uint32_t>(start_address);
    while(!halt) {
      uint32_t new_pc = machine->run(pc);
      if(new_pc == 0x30000000)
        new_pc = static_cast<uint32_t>(dispatch_system_call());
      pc = new_pc;
    }
    printf("Thread halt\n");
  }
  catch(std::exception& e) {
    machine->backtrace();
    fprintf(stderr, "Exception: %s\n", e.what());
  }
  printf("%lld instructions run\n", static_cast<long long>(machine->instruction_count));
  machine = nullptr;
  try {
    process->unregister_task(this);
  }
  catch(std::exception& e) {
    fprintf(stderr, "Exception during unregister: %s\n", e.what());
  }
  machine_for_backtrace = nullptr;
}

int32_t OSTask::dispatch_system_call() {
  printf("dispatchSystemCall: %08X\n", machine->wsp);
  int32_t address = static_cast<int32_t>(
    memory->read_wide((machine->wsp & 0x7FFFFFFF) - 2));
  int32_t call = static_cast<int32_t>(
    memory->read_word(static_cast<uint32_t>(address)));
  int32_t return_address = machine->ac[3];

  OSContext* context = OSContext::context_for_call(call, process, this, memory, machine);
  context->ac0 = machine->ac[0];
  context->ac1 = machine->ac[1];
  context->ac2 = machine->ac[2];

  if(process->system_call_logging) {
    printf("System Call %o, called from %08X\n", call, address - 2);
    printf(" ac0=%08X\n", machine->ac[0]);
    printf(" ac1=%08X\n", machine->ac[1]);
    printf(" ac2=%08X\n", machine->ac[2]);
  }

  if(Trace::enabled("scalls")) {
    char buf[128];
    snprintf(buf, sizeof(buf), "tid=%d call=%s ac0=%08X ac1=%08X ac2=%08X",
             tid, Trace::call_name(call).c_str(),
             machine->ac[0], machine->ac[1], machine->ac[2]);
    Trace::line("scalls", process->instance_label, buf);
  }

  // Terminal syscall (?RETURN 0310): the process is leaving the world
  // by a never-returning call — keyed on the NUMBER so every game site
  // is covered, including any hidden in disassembly holes. Retire the
  // pairing first; both engines then execute their authentic exits.
  if(call == OSContext::RETURN && hw::Lockstep::enabled &&
     (machine->lockstep_role == hw::Lockstep::MASTER ||
      machine->lockstep_role == hw::Lockstep::CLONE))
    hw::Lockstep::retire_ordinal(machine);

  int32_t error;
  hw::Lockstep::task_thread_label = process->instance_label.c_str();
  if(LockstepMediator::applies(this))
    error = LockstepMediator::dispatch(this, call, context,
                                       static_cast<uint32_t>(address - 2));
  else
    error = context->dispatch_system_call(call);
  hw::Lockstep::task_thread_label = nullptr;

  if(Trace::enabled("scalls")) {
    char buf[128];
    snprintf(buf, sizeof(buf), "tid=%d call=%s ret err=%04X ac0=%08X ac1=%08X",
             tid, Trace::call_name(call).c_str(), error,
             context->ac0, context->ac1);
    Trace::line("scalls", process->instance_label, buf);
  }

  if(process->system_call_logging)
    printf("RETURNING AC1=%08X\n", context->ac1);

  machine->set_psr(static_cast<int32_t>(static_cast<uint32_t>(machine->wide_pop()) >> 16));
  if(error == SUCCESS) {
    machine->ac[0] = context->ac0;
    machine->ac[1] = context->ac1;
    machine->ac[2] = context->ac2;
    delete context;
    return return_address + 1;
  }
  else {
    printf("****** ERROR RETURN ******  CODE=%04X\n\n", error);
    machine->ac[0] = error;
    delete context;
    return return_address;
  }
}

} // namespace os
