#pragma once
#include <atomic>
#include <cstdint>
#include <map>
#include <mutex>


namespace hw {
class QueueEntry;
class Machine;
struct QueueEntry;
class Page;

// Lockstep scheduler policy (docs/LockstepHarness.md).
//
// Roles live on Machine (lockstep_role / lockstep_ordinal), propagated from
// OSProcess at task registration. MachineThread consults this policy when
// -lockstep is enabled: master/clone batches run as verified pairs (matching
// task ordinals, master first, nothing in between); server batches run
// between pairs, or free-run while both clients are blocked. A client
// pended inside a syscall has no queued batch, so "blocked" falls out of
// queue contents.

class Lockstep {
public:
  static constexpr int32_t NONE   = 0;  // unassigned (warn; treat as SERVER)
  static constexpr int32_t SERVER = 1;
  static constexpr int32_t MASTER = 2;
  static constexpr int32_t CLONE  = 3;

  static bool enabled;

  // Ruling 8 garbage probe (-zero=clone, docs/Project10): master
  // unzeroed vs clone zeroed. Register VALUES legitimately differ
  // wherever 1988 garbage flowed into a register, so compare_pair
  // relaxes the register-value comparison ONLY. pc, instruction
  // counts (existing exemption rules), terminal/span structure, trap
  // sites, exceptions, and syscall mediation stay armed — the user's
  // detector suite. A hit names a located read-of-uninitialized bug.
  static bool probe_relax_regs;

  // Serializes task-thread writes to shared pages against client pair
  // execution: the MachineThread worker holds this for the span of each
  // master+clone pair, and any syscall handler (task thread) writing a
  // page takes it briefly per write. This keeps the two halves of a pair
  // observing identical shared-memory state without letting a blocking
  // handler hold a lock across a wait.
  static std::mutex shared_write_mutex;

  // True on the MachineThread worker (instruction execution); handler
  // threads leave it false and must take shared_write_mutex for writes.
  static thread_local bool on_worker_thread;

  // Set for the duration of a system call handler on its task thread, so
  // shared-page write tracing can attribute handler writes honestly
  // instead of blaming whichever batch is on the worker.
  static thread_local const char* task_thread_label;

  // Reentrant task-thread gate on shared_write_mutex. Engages only off the
  // worker thread while lockstep is enabled; nesting (e.g. MirrorPage
  // holding the gate across its two underlying page writes) is handled by
  // a per-thread depth count.
  struct WriteGate {
    bool engaged;
    WriteGate();
    ~WriteGate();
  };
  static thread_local int write_gate_depth;

  // Step 3 registry: clone-private copies of shared data pages, keyed by
  // the real (file) page. Registered when the clone maps its copies; the
  // server's mappings are then wrapped in MirrorPages that write both.
  static void register_copy(Page* real, Page* copy);
  static Page* copy_for(Page* real);

  // Pair-boundary page-diff audit: every PAGE_AUDIT_INTERVAL completed
  // pairs, byte-compare every registered (real, copy) page while the
  // worker still holds shared_write_mutex. At a pair boundary the two
  // must be identical (identical client writes, mirrored server writes,
  // replay completed before the clone's batch could queue), so any
  // difference is silent drift in bytes nobody has consumed yet.
  // 0 disables the audit.
  static constexpr uint32_t PAGE_AUDIT_INTERVAL = 16;
  static void maybe_audit_copies(QueueEntry* master, QueueEntry* clone);

  // Compare the two completed halves of a pair. On divergence: report both
  // engines' positions (labels, pcs, instruction counts, backtraces) and
  // abort. Emits a "lockstep" trace line per pair when tracing is enabled.
  static void compare_pair(QueueEntry* master, QueueEntry* clone);

  // Diagnostic when a batch has waited too long for its counterpart.
  static void report_waiting(QueueEntry* entry);

  // Terminal detach. When both halves of a pair end at a terminal entry
  // (RTStubs::terminal_bits / QUEST_TERMINAL), that pair is the LAST
  // verified event for the ordinal: compare_pair verifies it normally,
  // then detach() — the clone's tasks halt (no file writes; master pages
  // are canonical), the server's MirrorPage wrappings revert to the plain
  // real pages (otherwise the frozen clone copies would fire false
  // compare-on-read divergences), the copy registry and page audit
  // retire, the mediator passes the master's syscalls straight through,
  // and the scheduler runs the master's batches unpaired. The master then
  // runs the terminal path to completion unverified.
  static bool is_detached(int32_t ordinal);
  static void detach(QueueEntry* master, QueueEntry* clone);

  // abort_world — the deliberate stop-the-world path (Layering ruling 6
  // impl note + ruling 7). Used when pairing is impossible or the world
  // is proven corrupt: prints the reason (+ machine state if given),
  // silences the checker and stall diagnostics, halts every task,
  // wakes every parked thread so nothing hangs, and lets Launch's
  // normal teardown run. save=false suppresses the final FS::save_all
  // (DERR: state presumed corrupt). Idempotent. Safe to call from the
  // worker or a task thread; caller should throw afterward to unwind
  // its own execution.
  static void abort_world(const char* reason, Machine* machine, bool save);

  // Terminal syscall retirement (?RETURN 0310, keyed at dispatch): the
  // pair's LAST verified event was the syscall-gate rendezvous; both
  // processes are about to exit AUTHENTICALLY by their own ?RETURN, so
  // unlike detach() nobody is halted — this only unhooks the pairing
  // world first (detached flag → mediation bypass; server unmirror;
  // copy registry retired) so neither exit drags the other or the
  // server into a coupled teardown (the historical "Forced exit"
  // hang). Idempotent; either role may arrive first.
  static void retire_ordinal(Machine* machine);
  static std::atomic<bool> aborting;
  static std::atomic<bool> suppress_save;

private:
  static std::atomic<bool> detached[64];
  static void clear_copies();   // empty the copy registry (under detach)
};

} // namespace hw
