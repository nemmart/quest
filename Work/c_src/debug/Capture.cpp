// src/debug/Capture.cpp
#include "Capture.hpp"
#include "../hw/Machine.hpp"
#include "../os/OSProcess.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>

namespace debug {

namespace {
  bool initialized = false;
  bool enabled = false;
  uint32_t entry_pc = 0;
  // One-shot arming state. The game is non-reentrant (Plan.md), so a
  // single pending capture at a time is sufficient; a second entry
  // while armed is reported and ignored. Under lockstep the state is
  // shared across master and clone and relies on pair ordering: the
  // master's batch runs first, claims the ENTRY/RETURN pair, and the
  // clone (native-dispatching, so never at the entry pc) does not
  // interact with the arming at all — its snapshots come from
  // native_footprint. Fragile by design; fine for a derivation tool
  // (cross-review, docs/REVIEW_UNSIGNED_TO_CHAR.md).
  bool armed = false;
  uint32_t return_pc = 0;
  uint32_t region_base = 0;   // entry wsp-8, fixed for the A/B pair
  uint32_t dest_addr = 0;     // entry ac2 (word address), 0 = none
  uint32_t dest_override = 0; // QUEST_CAPTURE_DEST: fixed region instead of ac2
  int seq = 0;

  void init() {
    initialized = true;
    const char* env = std::getenv("QUEST_CAPTURE");
    if(!env) return;
    entry_pc = static_cast<uint32_t>(std::strtoul(env, nullptr, 16));
    enabled = entry_pc != 0;
    // Routines whose footprint lies outside the frame and the entry-ac2
    // region (e.g. I.LOCK writes only the lock object at 0x70000200)
    // need a fixed second window.
    const char* d = std::getenv("QUEST_CAPTURE_DEST");
    if(d) dest_override = static_cast<uint32_t>(std::strtoul(d, nullptr, 16));
    if(enabled)
      fprintf(stderr, "Capture: armed for entry %08X\n", entry_pc);
  }
}

void Capture::snapshot(hw::Machine& machine, const char* tag,
                       uint32_t base, uint32_t dest) {
  std::string name = "capture-" + machine.process->instance_label + ".txt";
  FILE* f = fopen(name.c_str(), "a");
  if(!f) return;
  fprintf(f, "=== seq=%d %s pc=%08X ===\n", seq, tag,
          static_cast<uint32_t>(machine.pc));
  fprintf(f, "ac0=%08X ac1=%08X ac2=%08X ac3=%08X\n",
          static_cast<uint32_t>(machine.ac[0]),
          static_cast<uint32_t>(machine.ac[1]),
          static_cast<uint32_t>(machine.ac[2]),
          static_cast<uint32_t>(machine.ac[3]));
  fprintf(f, "wsp=%08X wfp=%08X c=%d ovr=%d ovk=%d psr=%04X\n",
          static_cast<uint32_t>(machine.wsp),
          static_cast<uint32_t>(machine.wfp),
          machine.c, machine.ovr, machine.ovk,
          static_cast<uint32_t>(machine.get_psr()) & 0xFFFF);
  fprintf(f, "region base=%08X\n", base);
  for(int row = 0; row < 92; row += 8) {
    fprintf(f, "%08X:", base + row);
    for(int i = 0; i < 8 && row + i < 92; i++)
      fprintf(f, " %04X",
              machine.memory->read_word(base + row + i) & 0xFFFF);
    fprintf(f, "\n");
  }
  if(dest != 0) {
    fprintf(f, "dest base=%08X (byte %08X)\n", dest, dest * 2);
    for(int row = 0; row < 18; row += 8) {
      fprintf(f, "%08X:", dest + row);
      for(int i = 0; i < 8 && row + i < 18; i++)
        fprintf(f, " %04X",
                machine.memory->read_word(dest + row + i) & 0xFFFF);
      fprintf(f, "\n");
    }
  }
  fclose(f);
}

void Capture::native_footprint(hw::Machine& machine) {
  if(!initialized) init();
  if(!enabled) return;
  // At wrapper time (pre-native_return) wsp still holds the entry
  // value, so the region base formula matches the entry-side capture.
  uint32_t base = static_cast<uint32_t>(machine.wsp) - 8;
  uint32_t dest = dest_override ? dest_override : static_cast<uint32_t>(machine.ac[2]);
  snapshot(machine, "NATIVE", base, dest);
  seq++;
}

void Capture::check(hw::Machine& machine) {
  if(!initialized) init();
  if(!enabled) return;
  uint32_t pc = static_cast<uint32_t>(machine.pc);
  if(armed && pc == return_pc) {
    snapshot(machine, "RETURN", region_base, dest_addr);
    armed = false;
    seq++;
    return;
  }
  if(pc == entry_pc) {
    if(armed) {
      fprintf(stderr, "Capture: nested entry at %08X while armed — ignored\n", pc);
      return;
    }
    armed = true;
    return_pc = static_cast<uint32_t>(machine.ac[3]);
    region_base = static_cast<uint32_t>(machine.wsp) - 8;
    dest_addr = dest_override ? dest_override : static_cast<uint32_t>(machine.ac[2]);
    snapshot(machine, "ENTRY", region_base, dest_addr);
  }
}

} // namespace debug
