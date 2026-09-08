#pragma once
#include <cstdint>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>


namespace hw {
class Machine;
class QueueEntry;

// All instructions execute on a single thread to avoid locking individual
// memory pages.  Callers submit work via run_steps() which blocks until
// the batch completes.

class MachineThread {
public:
  // Abort support (Lockstep::abort_world): complete every parked entry
  // and wake every waiter so no thread is left hanging, then let the
  // worker exit via shutdown.
  static void abort_all();
public:
  // Machine whose instructions the worker is currently executing (worker
  // thread only; used for attributing traced memory writes to a process).
  static Machine* current_machine;

  MachineThread();
  ~MachineThread();

  uint32_t run_steps(Machine* machine, uint32_t address, int32_t steps);

private:
  void thread_main();

  // Lockstep selection (mtx held). Chooses the next batch per the policy
  // in docs/LockstepHarness.md; sets is_clone_half when returning the
  // second half of a master/clone pair. Returns nullptr when nothing is
  // runnable (unpaired client batches wait for their counterparts).
  QueueEntry* lockstep_pick_locked(bool& is_clone_half);
  bool lockstep_runnable_locked();
  void lockstep_stall_locked();

  std::thread worker;
  std::mutex mtx;
  std::condition_variable queue_cv;
  std::deque<QueueEntry*> queue;
  bool shutdown;

  // Lockstep worker state (worker thread only; queue ops under mtx)
  QueueEntry* clone_next = nullptr;    // second half of the running pair
  QueueEntry* master_half = nullptr;   // completed master awaiting its clone (member for abort_all)
  bool warned_none_role = false;
  bool stall_reported = false;
};

} // namespace hw
