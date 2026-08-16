#include "OSContextSystem.hpp"
#include "OSProcess.hpp"
#include "OSTask.hpp"
#include "OSError.hpp"
#include "ArrayPage.hpp"
#include "../hw/Memory.hpp"
#include "../hw/Machine.hpp"
#include "../hw/Permissions.hpp"
#include <cstdio>
#include <ctime>
#include <chrono>
#include <thread>
#include <algorithm>
#include <stdexcept>




namespace os {
using namespace hw;

OSContextSystem::OSContextSystem(OSProcess* p, OSTask* t, Memory* m, Machine* mc)
  : OSContext(p, t, m, mc) {}

int32_t OSContextSystem::dispatch_system_call(int32_t call) {
  switch(call) {
    case RECREATE: return RECREATE_call();
    case MEM:      return MEM_call();
    case MEMI:     return MEMI_call();
    case GTOD:     return GTOD_call();
    case PNAME:    return PNAME_call();
    case RNGPR:    return RNGPR_call();
    case ERMSG:    return ERMSG_call();
    case DADID:    return DADID_call();
    case RETURN:   return RETURN_call();
    case IXIT:     return IXIT_call();
    case INTWT:    return INTWT_call();
    case WDELAY:   return WDELAY_call();
  }
  throw std::runtime_error("Dispatch system call - missing case");
}

// ?RNGPR — "Returns the .PR filename for a ring." (AOS/VS)
//
//   AC0  PID, or a byte pointer to a process name, or -1 for self
//   AC1  -1 when AC0 is a byte pointer; anything else when AC0 is a PID or -1
//   AC2  word pointer to the ?RNGPR packet
//   out  length of the returned string, in packet offset ?RNGLB
//
// Packet layout as used by ?FATAL at 0x7017F3E9-F3F9:
//   +0 (wide)  byte pointer to the result buffer
//   +2 (word)  ring number
//   +3 (word)  ?RNGLB — buffer size on input, string length on output
//              (?FATAL reads it straight back at 0x7017F400)
//
// ?FATAL uses this to turn a return address into a module name: it masks
// the address with 0x7000 and shifts right 12 to get the ring, then asks
// which .PR is loaded there. Everything Quest executes is ring 7, so any
// other ring is reported "ring not loaded" — which the caller already
// handles (error return at 0x7017F3FC branches to 0x7017F45D and simply
// omits the name).
//
// Only reached on the terminal ?FATAL path, so this can never contribute
// to a lockstep divergence.
int32_t OSContextSystem::RNGPR_call() {
  static constexpr int32_t QUEST_RING = 7;

  int32_t buffer = mem_read_wide(ac2);
  int32_t ring   = mem_read_word(ac2 + 2);
  int32_t room   = mem_read_word(ac2 + 3);

  printf("\n?RNGPR:\n   ring = %d\n   buffer = %08X\n   room = %d\n",
         ring, buffer, room);

  if(ring != QUEST_RING) {
    printf("   ring not loaded\n\n");
    return static_cast<int32_t>(aos_symbol("ERRNL"));   // ring not loaded
  }

  std::string name = ":" + process->program + ".PR";
  if(static_cast<int32_t>(name.size()) + 1 > room) {
    printf("   insufficient room in buffer\n\n");
    return static_cast<int32_t>(aos_symbol("ERIRB"));   // insufficient room
  }

  write_string(name, buffer);
  mem_write_word(ac2 + 3, static_cast<int32_t>(name.size()));
  printf("   name = %s\n\n", name.c_str());
  return SUCCESS;
}

// ?ERMSG - "Reads the error message file." (AOS/VS)
//
//   AC0  error code (a 32-bit unsigned value)
//   AC1  DG bits 16-23 = byte length of the message buffer
//        DG bits 24-31 = channel of the error message file; 0377 selects
//                        the system default ERMES file
//   AC2  byte pointer to the message buffer
//   out  AC0 = byte length of the message, AC1 = channel actually used,
//        AC2 unchanged
//
// Quest passes AC1 = 0x0000FFFF at 0x7017F343, which decodes as a
// 255-byte buffer on channel 0377 - the default ERMES file. (DG numbers
// bits MSB-first, so "bits 16-23" are C bits 15-8.)
//
// Used by ?FATAL at 0x7017F346 to name the condition that killed the
// process. The caller stores the returned length at [ac3+0xC] and
// substitutes 0 on the error return, so a failure yields an unnamed
// condition rather than breaking the report.
//
// We have no ERMES file. Codes the emulator knows are rendered from its
// own table; for anything else the documented behaviour is a NORMAL
// return carrying "UNKNOWN ERROR CODE n", which is what we emit - the
// caller then prints the code, which is what a traceback needs anyway.
int32_t OSContextSystem::ERMSG_call() {
  int32_t room    = (ac1 >> 8) & 0xFF;   // DG bits 16-23
  int32_t channel = ac1 & 0xFF;          // DG bits 24-31 (0377 = ERMES)

  std::string text;
  try {
    text = OSError::message_for_error(ac0);
  }
  catch(std::exception&) {
    char buf[64];
    snprintf(buf, sizeof(buf), "UNKNOWN ERROR CODE %d", ac0);
    text = buf;
  }

  printf("\n?ERMSG:\n   code = %o (0x%X)\n   channel = %o\n   room = %d\n   text = %s\n\n",
         ac0, ac0, channel, room, text.c_str());

  if(static_cast<int32_t>(text.size()) > room)
    return static_cast<int32_t>(aos_symbol("ERTXT"));   // text longer than buffer

  write_string(text, ac2);
  ac0 = static_cast<int32_t>(text.size());
  ac1 = channel;
  return SUCCESS;
}

int32_t OSContextSystem::RECREATE_call() {
  printf("\nRECREATE:\n   name = %s\n\n", read_string(ac0).c_str());
  return SUCCESS;
}

int32_t OSContextSystem::MEM_call() {
  ac0 = process->shared_start - process->unshared_stop;
  ac1 = process->unshared_stop;

  printf("\nMEM (return values)\n  ac0 = %d\n  ac1 = %d\n  ac2 = %08X\n\n", ac0, ac1, ac2);
  return SUCCESS;
}

int32_t OSContextSystem::MEMI_call() {
  if(ac0 > 0) {
    int32_t count = ac0;
    while(count > 0) {
      memory->map_page(new ArrayPage(),
        process->unshared_stop + OSProcess::SEGMENT_BASE,
        Permissions::PERMISSIONS_READ_WRITE_EXECUTE);
      process->unshared_stop++;
      count--;
    }
    ac1 = ((process->unshared_stop + OSProcess::SEGMENT_BASE) << 10) - 1;
    printf("\nMEMI (return values)\n  ac1 = %08X\n\n", ac1);
    return SUCCESS;
  }
  return OSError::OS_NOT_IMPLEMENTED;
}

int32_t OSContextSystem::GTOD_call() {
  time_t now = time(nullptr);
  struct tm* tm = localtime(&now);
  ac0 = tm->tm_sec;
  ac1 = tm->tm_min;
  ac2 = tm->tm_hour;
  return SUCCESS;
}

int32_t OSContextSystem::PNAME_call() {
  printf("\nPNAME:\n   ac0 = %08X\n   ac1 = %08X\n\n", ac0, ac0);
  if(ac1 == -1 && ac0 == 0) {
    ac1 = process->pid;
    return SUCCESS;
  }
  return OSError::OS_NOT_IMPLEMENTED;
}

int32_t OSContextSystem::DADID_call() {
  ac1 = 1;
  return SUCCESS;
}

int32_t OSContextSystem::RETURN_call() {
  printf("TERMINATING PROCESS\n");
  if((ac2 & 0xFF) != 0)
    printf("   message = %s\n", read_string(ac1, ac2 & 0xFF).c_str());
  else
    printf("   no termination message\n");

  if((ac2 & 0xFF) != 0)
    fprintf(stderr, "termination message = %s\n", read_string(ac1, ac2 & 0xFF).c_str());
  else
    fprintf(stderr, "no termination message\n");

  process->terminating = true;
  {
    std::lock_guard<std::recursive_mutex> lock(process->task_mutex);
    for(size_t i = 0; i < process->tasks.size(); i++) {
      if(process->tasks[i] && process->tasks[i] != task)
        process->tasks[i]->halt_task();
    }
  }
  while(true) {
    if(process->count_tasks() > 1) {
      fprintf(stderr, "Waiting for tasks to terminate\n");
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    else
      break;
  }
  throw std::runtime_error("EXIT!");
}

int32_t OSContextSystem::IXIT_call() {
  printf("\nIXIT -- ignored\n\n");
  return SUCCESS;
}

int32_t OSContextSystem::INTWT_call() {
  printf("\nINTWT\n\n");
  while(!task->halt) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  throw std::runtime_error("INTWT interrupted");
}

int32_t OSContextSystem::WDELAY_call() {
  printf("\nWDELAY FOR %d MS\n\n", ac0);
  int32_t remaining = ac0;
  while(remaining > 0 && !task->halt) {
    int32_t chunk = std::min(remaining, (int32_t)100);
    std::this_thread::sleep_for(std::chrono::milliseconds(chunk));
    remaining -= chunk;
  }
  return SUCCESS;
}

} // namespace os
