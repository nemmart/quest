#include "LockstepMediator.hpp"
#include "OSContext.hpp"
#include "OSTask.hpp"
#include "OSProcess.hpp"
#include "Trace.hpp"
#include "ProbeSuppressions.hpp"
#include "../hw/Lockstep.hpp"
#include "../hw/Machine.hpp"
#include "../hw/Memory.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>


namespace os {

std::atomic<bool> LockstepMediator::released{false};

void LockstepMediator::release_all() {
  released.store(true);
  std::lock_guard<std::mutex> lock(slots_mutex);
  for(auto& [ord, s] : slots) {
    if(!s) continue;
    std::lock_guard<std::mutex> g(s->m);
    s->cv.notify_all();
  }
}

using hw::Lockstep;

std::mutex LockstepMediator::slots_mutex;
std::map<int32_t, LockstepMediator::Slot*> LockstepMediator::slots;

bool LockstepMediator::applies(OSTask* task) {
  if(!Lockstep::enabled || !task->machine)
    return false;
  int32_t role = task->machine->lockstep_role;
  if(role != Lockstep::MASTER && role != Lockstep::CLONE)
    return false;
  // After terminal detach the clone is halted and will never arrive at a
  // rendezvous; the master dispatches directly (unverified by design).
  if(Lockstep::is_detached(task->machine->lockstep_ordinal))
    return false;
  return true;
}

LockstepMediator::Slot& LockstepMediator::slot_for(int32_t ordinal) {
  std::lock_guard<std::mutex> lock(slots_mutex);
  Slot*& s = slots[ordinal];
  if(!s)
    s = new Slot();
  return *s;
}

bool LockstepMediator::is_local(int32_t call) {
  switch(call) {
    // Deterministic process-structure calls: both clients execute their own.
    case OSContext::MEM:    case OSContext::MEMI:
    case OSContext::GSHPT:  case OSContext::SSHPT:
    case OSContext::SOPEN:  case OSContext::SPAGE:
    case OSContext::TASK:   case OSContext::REC:
    case OSContext::KILAD:  case OSContext::UIDSTAT:
    case OSContext::INTWT:  case OSContext::WDELAY:
    case OSContext::RETURN: case OSContext::RECREATE:
    case OSContext::IXIT:
      return true;
    // World-facing calls: master executes, clone gets checked copies.
    case OSContext::GTOD:   case OSContext::PNAME:  case OSContext::DADID:
    case OSContext::OPEN:   case OSContext::CLOSE:
    case OSContext::READ:   case OSContext::WRITE:  case OSContext::UPDATE:
    case OSContext::CREATE: case OSContext::SERVE:  case OSContext::ILKUP:
    case OSContext::IREC:   case OSContext::ISEND:  case OSContext::ISR:
    case OSContext::CON:    case OSContext::DCON:
      return false;
  }
  fprintf(stderr, "Lockstep: system call %04o has no LOCAL/MEDIATED "
                  "classification\n", call);
  fflush(nullptr);
  abort();
}

void LockstepMediator::verify_arrival(Slot& s) {
  // Control-flow compare: call numbers always; SITES only when both are
  // trap-derived (master always is — no stubs; the clone's native-wrapper
  // sites are entry pcs, verified by the crossings entry pair instead).
  // The suppression key is the MASTER's site — uniformly derived, and the
  // key specimen #1 was recorded under.
  bool flow_mismatch = s.master_call != s.clone_call ||
                       (!s.clone_site_native && s.master_site != s.clone_site);
  // Generation 4 (M4a): a clone AC that is an area/shifted-stack address
  // passes when its T()-translation equals the master's value.
  hw::Machine* cm = s.clone_task ? s.clone_task->machine : nullptr;
  uint32_t reg_mask = 0;
  for(int i = 0; i < 3; i++) {
    bool differ = cm
      ? cm->equivalent(static_cast<uint32_t>(s.master_ac[i]),
                       static_cast<uint32_t>(s.clone_ac[i])).kind == hw::Mapper::Kind::MISMATCH
      : s.master_ac[i] != s.clone_ac[i];
    if(differ)
      reg_mask |= 1u << i;
  }
  bool mismatch = flow_mismatch || reg_mask != 0;
  if(!mismatch) {
    s.recent_sites[s.recent_next] = s.master_site;   // agreed site
    s.recent_next = (s.recent_next + 1) % 8;
    return;
  }

  // PROBE MODE (Project 11, -zero=clone only): register-VALUE mismatches
  // never halt — master authoritative, collect-don't-halt. Halts remain
  // for the control-flow family: call-number or SITE-pc disagreement
  // (two engines issuing from different places is a pc fork wearing
  // syscall clothing) — fall through to the halt below with sites shown.
  // Outside probe mode this block is unreachable: shipping and attic
  // semantics untouched.
  if(Lockstep::probe_relax_regs && !flow_mismatch) {
    const ProbeSuppression* known =
      ProbeSuppressions::find(s.master_call, s.master_site, reg_mask);
    if(known)
      ProbeSuppressions::log_known(known, s.master_call, s.master_site,
                                   s.master_ac, s.clone_ac);
    else
      ProbeSuppressions::forensic_record(s.master_call, s.master_site,
                                         s.master_task, s.clone_task,
                                         s.master_ac, s.clone_ac, reg_mask,
                                         is_local(s.master_call),
                                         s.recent_sites, 8);
    s.recent_sites[s.recent_next] = s.master_site;
    s.recent_next = (s.recent_next + 1) % 8;
    return;   // continue: mediated calls consume master results downstream
  }
  fflush(stdout);
  fprintf(stderr, "LOCKSTEP DIVERGENCE - details in stdout log\n");
  printf("\n================ LOCKSTEP DIVERGENCE ================\n");
  printf("rendezvous mismatch (call or arguments):\n");
  printf("  master: call=%04o site=%08X ac0=%08X ac1=%08X ac2=%08X\n",
          s.master_call, s.master_site,
          s.master_ac[0], s.master_ac[1], s.master_ac[2]);
  printf("  clone : call=%04o site=%08X ac0=%08X ac1=%08X ac2=%08X\n",
          s.clone_call, s.clone_site,
          s.clone_ac[0], s.clone_ac[1], s.clone_ac[2]);
  printf("master backtrace:\n");
  if(s.master_task && s.master_task->machine)
    s.master_task->machine->backtrace();
  printf("clone backtrace:\n");
  if(s.clone_task && s.clone_task->machine)
    s.clone_task->machine->backtrace();
  printf("=====================================================\n");
  fflush(nullptr);
  abort();
}

int32_t LockstepMediator::dispatch(OSTask* task, int32_t call, OSContext* context,
                                   uint32_t site_pc, bool site_native) {
  bool is_master = task->machine->lockstep_role == Lockstep::MASTER;
  Slot& s = slot_for(task->machine->lockstep_ordinal);

  std::unique_lock<std::mutex> lock(s.m);

  // Arrivals allowed while no mediated cycle is in flight and our own
  // flag is clear (the counterpart's previous cycle may still be winding
  // down; cycle/phase keep the generations separate).
  s.cv.wait(lock, [&]{ return released.load() || (s.phase == 0 &&
                              !(is_master ? s.master_here : s.clone_here)); });

  if(is_master) {
    s.master_here = true;
    s.master_call = call;
    s.master_task = task;
    s.master_site = site_pc;
    s.master_ac[0] = context->ac0;
    s.master_ac[1] = context->ac1;
    s.master_ac[2] = context->ac2;
  }
  else {
    s.clone_here = true;
    s.clone_call = call;
    s.clone_task = task;
    s.clone_site = site_pc;
    s.clone_site_native = site_native;
    s.clone_ac[0] = context->ac0;
    s.clone_ac[1] = context->ac1;
    s.clone_ac[2] = context->ac2;
  }

  bool closer = s.master_here && s.clone_here;
  if(closer) {
    // Second arriver: both sides' data are valid under this lock hold.
    verify_arrival(s);
    if(is_local(call)) {
      s.master_here = s.clone_here = false;
      s.master_task = s.clone_task = nullptr;
      s.cycle++;                 // releases the first arriver
    }
    else {
      s.phase = 1;               // opens the mediated cycle
      s.writes.clear();
      s.results_ready = false;
    }
    s.cv.notify_all();
  }
  else {
    // First arriver: wait to be released by the closer, with diagnostics.
    uint64_t my_cycle = s.cycle;
    while(s.cycle == my_cycle && s.phase == 0) {
      if(s.cv.wait_for(lock, std::chrono::seconds(30),
          [&]{ return s.cycle != my_cycle || s.phase != 0; }))
        break;
      fprintf(stderr, "Lockstep: %s waiting for counterpart at rendezvous: "
                      "ordinal=%d call=%04o (%s)\n",
              is_master ? "master" : "clone",
              task->machine->lockstep_ordinal, call,
              Trace::call_name(call).c_str());
    }
  }

  if(is_local(call)) {
    // Cycle already closed (flags cleared, cycle bumped); both sides
    // execute their own call outside the lock.
    lock.unlock();
    return context->dispatch_system_call(call);
  }

  // MEDIATED (phase == 1 for both sides here)
  if(is_master) {
    context->write_log = &s.writes;
    s.master_memory = context->memory;
    context->read_verify = s.clone_task->process->memory;
    context->read_verify_task = s.clone_task;
    lock.unlock();
    int32_t error = context->dispatch_system_call(call);
    lock.lock();
    context->read_verify = nullptr;
    context->read_verify_task = nullptr;
    context->write_log = nullptr;
    s.r_error = error;
    s.r_ac0 = context->ac0;
    s.r_ac1 = context->ac1;
    s.r_ac2 = context->ac2;
    s.results_ready = true;
    s.cv.notify_all();
    return error;
  }
  else {
    s.cv.wait(lock, [&]{ return released.load() || s.results_ready; });
    // Replay the master's caller-memory writes into the clone -- but skip
    // writes to pages the clone physically shares with the master (shared
    // data mappings). The master's original write already landed on the
    // common page, and re-applying it later would stomp newer server
    // updates (e.g. regress the shared mailbox counters the game polls).
    hw::Memory* mm = static_cast<hw::Memory*>(s.master_memory);
    auto physically_shared = [&](uint32_t page_number) {
      hw::Page* cp = context->memory->find_page(page_number);
      return cp != nullptr && cp == mm->find_page(page_number);
    };
    // Generation 4 (M4a): replay lands at the CLONE's address for the
    // master's — clone_location (Mapper.md §1.3, form-aware: the codec
    // decomposes byte and @-flagged forms) maps a master stack address
    // that the clone keeps in an area (or shifted on its real stack)
    // back to the clone's cell.
    hw::Machine* cm = task->machine;
    for(const MediatedWrite& w : s.writes) {
      if(w.delivered)      // funnel dual-write already reached the clone copy;
        continue;          // re-applying would stomp newer server updates
      uint32_t pn = (w.width == 1) ? (w.address >> 11)
                                   : ((w.address >> 10) & 0x1FFFFF);
      uint32_t pn2 = (w.width == 4) ? (((w.address + 1) >> 10) & 0x1FFFFF) : pn;
      if(physically_shared(pn) && physically_shared(pn2))
        continue;
      uint32_t addr = cm ? cm->clone_location(w.address) : w.address;
      switch(w.width) {
        case 1: context->memory->write_byte(addr, w.value); break;
        case 2: context->memory->write_word(addr, w.value); break;
        case 4: context->memory->write_wide(addr, w.value); break;
      }
    }
    context->ac0 = s.r_ac0;
    context->ac1 = s.r_ac1;
    context->ac2 = s.r_ac2;
    int32_t error = s.r_error;
    // Clone consumption closes the mediated cycle.
    s.results_ready = false;
    s.master_here = s.clone_here = false;
    s.master_task = s.clone_task = nullptr;
    s.writes.clear();
    s.phase = 0;
    s.cycle++;
    s.cv.notify_all();
    return error;
  }
}

} // namespace os
