#pragma once
#include <cstdint>
#include <condition_variable>
#include <atomic>
#include <map>
#include <mutex>
#include <vector>


namespace os {

class OSTask;
class OSContext;

// Syscall rendezvous + mediation (docs/LockstepHarness.md).
//
// Every master/clone system call rendezvouses here: both tasks (paired by
// task ordinal) must arrive at the same call with the same ACs, or it's a
// divergence. Then:
//   LOCAL    — both execute their own call (deterministic process-structure
//              calls: memory, tasks, mappings, delays).
//   MEDIATED — the master executes once; result ACs, the error code, and
//              all caller-memory writes are captured and replayed into the
//              clone, which never executes the call. World-facing calls:
//              terminal/file I/O, IPC, identity, time.
// Unknown calls are a hard error, so the classification cannot silently
// drift. All waiting parks the calling task thread — never the
// MachineThread worker.

class LockstepMediator {
public:
  // Shutdown release (Aug 14, the ctrl-C wedge): wake every task
  // parked in a Slot rendezvous so shutdown_all's halt flags can be
  // observed. All wait predicates must also accept 'released'.
  static void release_all();
  static std::atomic<bool> released;
  // Route a client system call. Returns the error code to hand back to
  // the normal return path; on the clone, context ACs are already set
  // from the master's results. site_pc = the game-side syscall issue
  // site (address-2 in OSTask::dispatch_system_call — the pc "System
  // Call %o, called from %08X" logs); the probe suppression key
  // (Project 11) and part of the control-flow compare in probe mode.
  // site_native: the clone's native-wrapper path (RTBridge::syscall)
  // reports its ENTRY pc, not a trap pc — site equality is not a sound
  // halt criterion there (the crossings checker's entry pair already
  // verified the span's control flow). Master never sets it (no stubs).
  static int32_t dispatch(OSTask* task, int32_t call, OSContext* context,
                          uint32_t site_pc, bool site_native = false);

  // True if this task belongs to a lockstep client (master or clone).
  static bool applies(OSTask* task);

private:
  struct Slot {
    std::mutex m;
    std::condition_variable cv;
    // Cycle protocol: arrivals allowed while phase==0 and own flag clear.
    // The second arriver ("closer") verifies both sides' call+ACs, then
    // either releases a LOCAL cycle (clear flags, ++cycle) or opens a
    // MEDIATED one (phase=1; the clone's consumption closes it). The
    // first arriver waits for cycle to advance or phase to open.
    uint64_t cycle = 0;         // completed-cycle counter
    int phase = 0;              // 0 arrivals allowed, 1 mediated in flight
    bool master_here = false, clone_here = false;
    int32_t master_call = 0, clone_call = 0;
    int32_t master_ac[3] = {0,0,0}, clone_ac[3] = {0,0,0};
    uint32_t master_site = 0, clone_site = 0;   // syscall issue sites (Project 11)
    bool clone_site_native = false;             // clone site is a native entry pc
    uint32_t recent_sites[8] = {0};             // ring: last verified rendezvous sites
    int recent_next = 0;
    OSTask* master_task = nullptr;
    OSTask* clone_task = nullptr;
    // mediated results
    void* master_memory = nullptr;  // hw::Memory* of the master (page identity checks)
    bool results_ready = false;
    int32_t r_error = 0, r_ac0 = 0, r_ac1 = 0, r_ac2 = 0;
    std::vector<struct MediatedWrite> writes;
  };

  static Slot& slot_for(int32_t ordinal);
  static bool is_local(int32_t call);
  static void verify_arrival(Slot& s);

  static std::mutex slots_mutex;
  static std::map<int32_t, Slot*> slots;
};

} // namespace os
