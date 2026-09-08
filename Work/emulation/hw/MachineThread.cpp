#include "MachineThread.hpp"
#include "QueueEntry.hpp"
#include "Lockstep.hpp"
#include "Machine.hpp"
#include <stdexcept>
#include <algorithm>
#include <chrono>
#include <climits>
#include <cstdio>




namespace hw {
Machine* MachineThread::current_machine = nullptr;

MachineThread::MachineThread() : shutdown(false) {
  worker = std::thread(&MachineThread::thread_main, this);
}

void MachineThread::abort_all() {
  MachineThread* mt = Machine::machine_thread;
  if(!mt)
    return;
  {
    std::lock_guard<std::mutex> lock(mt->mtx);
    mt->shutdown = true;
    // Complete every parked half and every queued entry so their
    // submitting task threads wake, observe their halt flags
    // (OS::shutdown_all set them), and exit their loops cleanly.
    if(mt->master_half) { mt->master_half->completed = true; mt->master_half = nullptr; }
    if(mt->clone_next)  { mt->clone_next->completed = true;  mt->clone_next  = nullptr; }
    for(QueueEntry* e : mt->queue)
      e->completed = true;
    mt->queue.clear();
  }
  mt->queue_cv.notify_all();
}

MachineThread::~MachineThread() {
  {
    std::lock_guard<std::mutex> lock(mtx);
    shutdown = true;
  }
  queue_cv.notify_all();
  if(worker.joinable())
    worker.join();
}

// Lockstep policy (docs/LockstepHarness.md): master/clone batches run as
// verified pairs — master first, clone immediately after, nothing between
// them. Server batches run between pairs, or continuously while neither
// client has a batch queued (both blocked). Unpaired client batches wait
// for their counterparts; persistent asymmetry is reported.

QueueEntry* MachineThread::lockstep_pick_locked(bool& is_clone_half) {
  is_clone_half = false;

  // Second half of a pair in progress: run it before anything else.
  if(clone_next) {
    QueueEntry* e = clone_next;
    clone_next = nullptr;
    is_clone_half = true;
    return e;
  }

  // Lowest-ordinal complete master/clone pair.
  QueueEntry* best_master = nullptr;
  QueueEntry* best_clone = nullptr;
  int32_t best_ordinal = INT_MAX;
  for(QueueEntry* m : queue) {
    if(m->machine->lockstep_role != Lockstep::MASTER)
      continue;
    int32_t ordinal = m->machine->lockstep_ordinal;
    if(ordinal >= best_ordinal)
      continue;
    for(QueueEntry* c : queue) {
      if(c->machine->lockstep_role == Lockstep::CLONE &&
         c->machine->lockstep_ordinal == ordinal) {
        best_master = m;
        best_clone = c;
        best_ordinal = ordinal;
        break;
      }
    }
  }
  if(best_master) {
    queue.erase(std::find(queue.begin(), queue.end(), best_master));
    queue.erase(std::find(queue.begin(), queue.end(), best_clone));
    clone_next = best_clone;
    return best_master;
  }

  // No pair ready: serve server work (free-runs while clients are blocked),
  // and any DETACHED master (its clone has halted at a terminal entry —
  // Lockstep::detach — so it runs unpaired from here on).
  for(auto it = queue.begin(); it != queue.end(); ++it) {
    int32_t role = (*it)->machine->lockstep_role;
    if(role == Lockstep::MASTER &&
       Lockstep::is_detached((*it)->machine->lockstep_ordinal)) {
      QueueEntry* e = *it;
      queue.erase(it);
      return e;
    }
    if(role == Lockstep::SERVER || role == Lockstep::NONE) {
      if(role == Lockstep::NONE && !warned_none_role) {
        warned_none_role = true;
        fprintf(stderr, "Lockstep: batch from process without a role; "
                        "treating as SERVER\n");
      }
      QueueEntry* e = *it;
      queue.erase(it);
      return e;
    }
  }
  return nullptr;   // only unpaired client batches — wait
}

bool MachineThread::lockstep_runnable_locked() {
  if(clone_next)
    return true;
  bool clone_seen[64] = {false};
  for(QueueEntry* e : queue) {
    int32_t role = e->machine->lockstep_role;
    if(role == Lockstep::SERVER || role == Lockstep::NONE)
      return true;
    if(role == Lockstep::MASTER &&
       Lockstep::is_detached(e->machine->lockstep_ordinal))
      return true;
    if(role == Lockstep::CLONE) {
      int32_t ordinal = e->machine->lockstep_ordinal;
      if(ordinal >= 0 && ordinal < 64)
        clone_seen[ordinal] = true;
    }
  }
  for(QueueEntry* e : queue) {
    if(e->machine->lockstep_role == Lockstep::MASTER) {
      int32_t ordinal = e->machine->lockstep_ordinal;
      if(ordinal >= 0 && ordinal < 64 && clone_seen[ordinal])
        return true;
    }
  }
  return false;
}

void MachineThread::lockstep_stall_locked() {
  if(stall_reported || queue.empty())
    return;
  stall_reported = true;
  for(QueueEntry* e : queue)
    Lockstep::report_waiting(e);
}

void MachineThread::thread_main() {
  Lockstep::on_worker_thread = true;
  bool pair_lock_held = false;         // shared_write_mutex held by worker
  while(true) {
    QueueEntry* entry = nullptr;
    bool is_clone_half = false;
    {
      std::unique_lock<std::mutex> lock(mtx);
      if(!Lockstep::enabled) {
        queue_cv.wait(lock, [this]{ return !queue.empty() || shutdown; });
        if(shutdown && queue.empty())
          return;
        entry = queue.front();
        queue.pop_front();
      }
      else {
        while(true) {
          entry = lockstep_pick_locked(is_clone_half);
          if(entry) {
            stall_reported = false;
            break;
          }
          if(shutdown && queue.empty())
            return;
          if(!queue_cv.wait_for(lock, std::chrono::seconds(10),
              [this]{ return lockstep_runnable_locked() || shutdown; }))
            lockstep_stall_locked();
        }
      }
    }
    if(entry) {
      // Hold the shared-write mutex for every batch so task-thread page
      // writes and clone page-copy snapshots (step 3) serialize with
      // instruction execution; a master half keeps it held through the
      // clone half so the pair observes one consistent shared state.
      if(Lockstep::enabled && !pair_lock_held) {
        Lockstep::shared_write_mutex.lock();
        pair_lock_held = true;
      }

      current_machine = entry->machine;
      entry->run();
      current_machine = nullptr;

      bool is_master_half = Lockstep::enabled && !is_clone_half &&
        entry->machine->lockstep_role == Lockstep::MASTER &&
        !Lockstep::is_detached(entry->machine->lockstep_ordinal);

      if(is_master_half) {
        // Hold the master's completion until its clone has run and the
        // pair has been compared: the submitting thread owns the entry's
        // storage, so releasing it early would also free it early.
        master_half = entry;
      }
      else if(Lockstep::enabled && is_clone_half) {
        Lockstep::compare_pair(master_half, entry);
        Lockstep::maybe_audit_copies(master_half, entry); // mutex still held
        Lockstep::shared_write_mutex.unlock(); // pair span complete
        pair_lock_held = false;
        std::lock_guard<std::mutex> lock(mtx);
        if(master_half) {
          master_half->completed = true;
          master_half = nullptr;
        }
        entry->completed = true;
        queue_cv.notify_all();
      }
      else {
        if(Lockstep::enabled && pair_lock_held) {
          Lockstep::shared_write_mutex.unlock();
          pair_lock_held = false;
        }
        std::lock_guard<std::mutex> lock(mtx);
        entry->completed = true;
        queue_cv.notify_all();
      }
    }
  }
}

uint32_t MachineThread::run_steps(Machine* machine, uint32_t address, int32_t steps) {
  QueueEntry entry(machine, address, steps);
  {
    std::lock_guard<std::mutex> lock(mtx);
    queue.push_back(&entry);
    queue_cv.notify_all();
  }
  // Wait for completion
  {
    std::unique_lock<std::mutex> lock(mtx);
    queue_cv.wait(lock, [&entry]{ return entry.completed; });
  }
  if(entry.exception) {
    std::runtime_error ex(*entry.exception);
    delete entry.exception;
    throw ex;
  }
  return entry.address;
}

} // namespace hw
