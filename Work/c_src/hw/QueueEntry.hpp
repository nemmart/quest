#pragma once
#include <cstdint>
#include <stdexcept>


namespace hw {
class Machine;

struct QueueEntry {
  Machine* machine;
  uint32_t address;
  int32_t steps;
  bool completed;
  std::runtime_error* exception;
  uint64_t insn_before;   // machine->instruction_count around run(),
  uint64_t insn_after;    // for lockstep pair comparison
  uint64_t block_after;   // machine->block_ordinal at batch end (Gen-6:
                          // the per-client block ordinal, compared at
                          // every pair — docs/Project22/BlockSyncDesign.md)
  bool native_span;       // batch contained a native runtime call (or the
                          // master's matching emulated body span): pairs
                          // rendezvous at the post-call point and exempt
                          // the instruction-count comparison
  bool terminal;          // batch ended at a terminal entry: after this
                          // pair verifies, the clone detaches
                          // (Lockstep::detach) and the master runs on

  QueueEntry(Machine* machine, uint32_t address, int32_t steps)
    : machine(machine), address(address), steps(steps),
      completed(false), exception(nullptr),
      insn_before(0), insn_after(0), block_after(0),
      native_span(false), terminal(false) {}

  void run();
};

} // namespace hw
