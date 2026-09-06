// src/debug/Capture.cpp
#include "Capture.hpp"
#include "../hw/Machine.hpp"
#include "../os/OSProcess.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <map>
#include <vector>

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
  uint32_t dest_override = 0; // QUEST_CAPTURE_DEST: fixed region instead of ac2
  uint32_t dest_ind = 0;      // QUEST_CAPTURE_DEST=@<addr>+<off>: base = M32[addr] + off (P33-C)
  uint32_t dest_off = 0;
  uint32_t dest_len = 18;     // QUEST_CAPTURE_LEN: words in the dest window (P33-C; default 18)
  // P33-C: extra windows, QUEST_CAPTURE_WINDOWS=<spec>[,<spec>...], spec =
  // <hex-base>:<hex-len> or @<addr>+<off>:<hex-len> (memory-oracle diffs of
  // whole regions — the shared data, the object pages)
  struct Win { uint32_t ind = 0, off = 0, base = 0, len = 0; };
  std::vector<Win> windows;
  int seq = 0;
  // P33-C: per-machine arming so a lockstep CLONE running the routine as
  // IR blocks gets its own ENTRY/RETURN pair (the original one-shot state
  // was shared and only the master's pair was recorded)
  struct Arm { bool armed = false; uint32_t return_pc = 0, region_base = 0, dest_addr = 0; };
  std::map<std::string, Arm> arms;

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
    if(d && d[0] == '@') {
      char* end;
      dest_ind = static_cast<uint32_t>(std::strtoul(d + 1, &end, 16));
      if(*end == '+') dest_off = static_cast<uint32_t>(std::strtoul(end + 1, nullptr, 16));
    } else if(d) dest_override = static_cast<uint32_t>(std::strtoul(d, nullptr, 16));
    const char* l = std::getenv("QUEST_CAPTURE_LEN");
    if(l) dest_len = static_cast<uint32_t>(std::strtoul(l, nullptr, 10));
    const char* w = std::getenv("QUEST_CAPTURE_WINDOWS");
    while(w && *w) {
      Win win; char* end;
      if(*w == '@') {
        win.ind = static_cast<uint32_t>(std::strtoul(w + 1, &end, 16));
        if(*end == '+') win.off = static_cast<uint32_t>(std::strtoul(end + 1, &end, 16));
      } else {
        win.base = static_cast<uint32_t>(std::strtoul(w, &end, 16));
      }
      if(*end == ':') win.len = static_cast<uint32_t>(std::strtoul(end + 1, &end, 16));
      windows.push_back(win);
      w = (*end == ',') ? end + 1 : end;
    }
    if(enabled)
      fprintf(stderr, "Capture: armed for entry %08X\n", entry_pc);
  }
}

uint32_t Capture::dest_of(hw::Machine& machine) {
  if(dest_ind) {
    uint32_t hi = machine.memory->read_word(dest_ind) & 0xFFFF;
    uint32_t lo = machine.memory->read_word(dest_ind + 1) & 0xFFFF;
    return ((hi << 16) | lo) + dest_off;
  }
  return dest_override ? dest_override : static_cast<uint32_t>(machine.ac[2]);
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
    for(uint32_t row = 0; row < dest_len; row += 8) {
      fprintf(f, "%08X:", dest + row);
      for(uint32_t i = 0; i < 8 && row + i < dest_len; i++)
        fprintf(f, " %04X",
                machine.memory->read_word(dest + row + i) & 0xFFFF);
      fprintf(f, "\n");
    }
  }
  for(const Win& win : windows) {
    uint32_t wb = win.base;
    if(win.ind) {
      uint32_t hi = machine.memory->read_word(win.ind) & 0xFFFF;
      uint32_t lo = machine.memory->read_word(win.ind + 1) & 0xFFFF;
      wb = ((hi << 16) | lo) + win.off;
    }
    fprintf(f, "window base=%08X len=%X\n", wb, win.len);
    for(uint32_t row = 0; row < win.len; row += 8) {
      fprintf(f, "%08X:", wb + row);
      for(uint32_t i = 0; i < 8 && row + i < win.len; i++)
        fprintf(f, " %04X", machine.memory->read_word(wb + row + i) & 0xFFFF);
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
  uint32_t dest = dest_of(machine);
  snapshot(machine, "NATIVE", base, dest);
  seq++;
}

void Capture::check(hw::Machine& machine) {
  if(!initialized) init();
  if(!enabled) return;
  uint32_t pc = static_cast<uint32_t>(machine.pc);
  Arm& a = arms[machine.process->instance_label];
  if(a.armed && pc == a.return_pc) {
    snapshot(machine, "RETURN", a.region_base, a.dest_addr);
    a.armed = false;
    seq++;
    return;
  }
  if(pc == entry_pc) {
    if(a.armed) {
      fprintf(stderr, "Capture: nested entry at %08X while armed — ignored\n", pc);
      return;
    }
    a.armed = true;
    a.return_pc = static_cast<uint32_t>(machine.ac[3]);
    a.region_base = static_cast<uint32_t>(machine.wsp) - 8;
    a.dest_addr = dest_of(machine);
    snapshot(machine, "ENTRY", a.region_base, a.dest_addr);
  }
}

} // namespace debug
