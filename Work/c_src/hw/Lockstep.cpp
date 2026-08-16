#include "../os/LockstepMediator.hpp"
#include "Lockstep.hpp"
#include "Page.hpp"
#include "QueueEntry.hpp"
#include "Machine.hpp"
#include "../os/OSProcess.hpp"
#include "../os/OS.hpp"
#include "MachineThread.hpp"
#include "RTStubs.hpp"
#include "Memory.hpp"
#include "../os/OSTask.hpp"
#include "../os/Trace.hpp"
#include "../debug/SymbolTable.hpp"
#include "../os/ArrayPage.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>


namespace hw {

bool Lockstep::enabled = false;
bool Lockstep::probe_relax_regs = false;   // -zero=clone (ruling 8 probe)
std::mutex Lockstep::shared_write_mutex;
thread_local bool Lockstep::on_worker_thread = false;
thread_local const char* Lockstep::task_thread_label = nullptr;
thread_local int Lockstep::write_gate_depth = 0;

Lockstep::WriteGate::WriteGate() {
  engaged = Lockstep::enabled && !Lockstep::on_worker_thread;
  if(engaged && Lockstep::write_gate_depth++ == 0)
    Lockstep::shared_write_mutex.lock();
}

Lockstep::WriteGate::~WriteGate() {
  if(engaged && --Lockstep::write_gate_depth == 0)
    Lockstep::shared_write_mutex.unlock();
}

static std::mutex copy_registry_mutex;
static std::map<Page*, Page*> copy_registry;
static void describe(const char* who, QueueEntry* e, QueueEntry* master_ref);

void Lockstep::register_copy(Page* real, Page* copy) {
  std::lock_guard<std::mutex> lock(copy_registry_mutex);
  copy_registry[real] = copy;
}

Page* Lockstep::copy_for(Page* real) {
  std::lock_guard<std::mutex> lock(copy_registry_mutex);
  auto it = copy_registry.find(real);
  return it == copy_registry.end() ? nullptr : it->second;
}

// Byte-compare one (real, copy) page. Fast path: both ArrayPages, memcmp
// the backing vectors; otherwise fall back to the byte read interface.
// Returns the first differing offset, or -1 when identical.
static int32_t first_page_diff(Page* real, Page* copy) {
  auto* ra = dynamic_cast<os::ArrayPage*>(real);
  auto* ca = dynamic_cast<os::ArrayPage*>(copy);
  if(ra && ca && ra->bytes.size() == ca->bytes.size()) {
    if(memcmp(ra->bytes.data(), ca->bytes.data(), ra->bytes.size()) == 0)
      return -1;
    for(size_t i = 0; i < ra->bytes.size(); i++)
      if(ra->bytes[i] != ca->bytes[i])
        return static_cast<int32_t>(i);
  }
  for(uint32_t i = 0; i < 2048; i++)
    if(real->read(i) != copy->read(i))
      return static_cast<int32_t>(i);
  return -1;
}

void Lockstep::maybe_audit_copies(QueueEntry* master, QueueEntry* clone) {
  if(PAGE_AUDIT_INTERVAL == 0)
    return;
  static uint32_t pairs_since_audit = 0;
  if(++pairs_since_audit < PAGE_AUDIT_INTERVAL)
    return;
  pairs_since_audit = 0;

  std::lock_guard<std::mutex> lock(copy_registry_mutex);
  for(auto& entry : copy_registry) {
    Page* real = entry.first;
    Page* copy = entry.second;
    int32_t off = first_page_diff(real, copy);
    if(off < 0)
      continue;

    fflush(stdout);
    fprintf(stderr, "LOCKSTEP DIVERGENCE - details in stdout log\n");
    printf("\n================ LOCKSTEP DIVERGENCE ================\n");
    const char* label = "?";
    if(auto* ap = dynamic_cast<os::ArrayPage*>(real))
      if(!ap->trace_label.empty())
        label = ap->trace_label.c_str();
    printf("pair-boundary page audit mismatch (silent shared-page drift):\n");
    printf("  page=%s, differing bytes (up to 8 shown):\n", label);
    int shown = 0;
    for(uint32_t i = static_cast<uint32_t>(off); i < 2048 && shown < 8; i++) {
      uint32_t r = real->read(i), c = copy->read(i);
      if(r != c) {
        printf("    off=0x%03X real=%02X clone_copy=%02X\n", i, r, c);
        shown++;
      }
    }
    describe("master", master, master);
    describe("clone ", clone, master);
    printf("(writer provenance: run with -trace FILE -types shared)\n");
    printf("=====================================================\n");
    fflush(nullptr);
    abort();
  }
}

static const char* label_of(QueueEntry* e) {
  if(e && e->machine && e->machine->process)
    return e->machine->process->instance_label.c_str();
  return "?";
}

static void describe(const char* who, QueueEntry* e, QueueEntry* master_ref) {
  uint64_t delta = e->insn_after - e->insn_before;
  hw::Machine* m = e->machine;
  hw::Machine* mm = master_ref->machine;
  uint32_t mapped[4];
  for(int i = 0; i < 4; i++)   // verdict decodings for the dump (Mapper.md §1.3)
    mapped[i] = m->equivalent(static_cast<uint32_t>(mm->ac[i]),
                              static_cast<uint32_t>(m->ac[i])).mapped;
  printf("  %s: label=%s ordinal=%d result_pc=%08X trap_pc=%08X insns=%llu"
         " ac0=%08X ac1=%08X ac2=%08X ac3=%08X c=%d"
         " wsp=%08X wfp=%08X psr=%04X shadow_wsp=%08X T(ac0..3)=%08X %08X %08X %08X%s%s\n",
          who, label_of(e), m->lockstep_ordinal,
          e->address, m->pc, static_cast<unsigned long long>(delta),
          m->ac[0], m->ac[1], m->ac[2], m->ac[3], m->c,
          m->wsp, m->wfp, m->get_psr(), m->shadow_wsp(),
          mapped[0], mapped[1], mapped[2], mapped[3],
          e->exception ? " exception=" : "",
          e->exception ? e->exception->what() : "");
}

void Lockstep::compare_pair(QueueEntry* master, QueueEntry* clone) {
  if(!master || !clone)
    return;

  uint64_t master_delta = master->insn_after - master->insn_before;
  uint64_t clone_delta  = clone->insn_after  - clone->insn_before;
  bool master_threw = master->exception != nullptr;
  bool clone_threw  = clone->exception  != nullptr;

  // Ruling 8 garbage probe (-zero=clone): register VALUES are the one
  // comparison the asymmetric configuration invalidates by design —
  // everything else below (pc/address, counts under the existing
  // exemption rules, terminal/span structure, trap sites, exceptions)
  // stays armed, and syscall mediation is untouched upstream.
  // Generation 4 (docs/CheckerHistory.md, M4aDesign.md §5): a register
  // passes if it equals the master's OR its T()-translation does — the
  // clone may hold an area/shifted-stack address where the master holds
  // the stack address. wsp: the clone's shadow (T of its own wsp) must
  // equal the master's wsp exactly — any missed area accounting fails
  // every subsequent pair.
  bool regs_differ = false;
  bool wsp_differs = false;
  if(!probe_relax_regs) {
    regs_differ = master->machine->c != clone->machine->c;
    for(int i = 0; i < 4 && !regs_differ; i++)
      regs_differ = clone->machine->equivalent(static_cast<uint32_t>(master->machine->ac[i]),
                                               static_cast<uint32_t>(clone->machine->ac[i]))
                    .kind == hw::Mapper::Kind::MISMATCH;
    wsp_differs = master->machine->wsp != clone->machine->shadow_wsp();
  }

  // Batches ending in a syscall trap both report the 0x30000000 sentinel
  // as their result pc; the real trap site survives in machine->pc (both
  // task threads are parked until this comparison completes). Compare it
  // so equal-AC syscalls made from different code paths can't slip by.
  bool trap_sites_differ =
    master->address == 0x30000000 && clone->address == 0x30000000 &&
    master->machine->pc != clone->machine->pc;

  // A native-span pair rendezvouses at the post-call point: instruction
  // counts legitimately differ (the clone skipped the emulated body), so
  // the delta comparison is exempted. One-sided spans are structural
  // divergence.
  if(aborting.load())
    return;

  // One-sided terminal arrival is structural divergence (same-address
  // one-sided arrival is impossible for emulated convergence, but a native
  // wrapper could mis-route — keep the check honest).
  bool terminal_mismatch = master->terminal != clone->terminal;

  // Generation 3 (docs/CheckerHistory.md): a terminal pair is just a
  // crossing. Counts are exempted exactly as at exit pairs — iff the
  // ARRIVING side skipped emulation natively. The clone's span flag is
  // the arrival-mode signal: native_transfer arrival sets it, emulated
  // arrival (depth-0 or fallback span) does not. The master's flag
  // cannot carry it — the master always arrives emulated, and its span
  // flag would conflate absorbing-for-native with absorbing-for-fallback
  // (the mv-attic both-emulated case). Hence at terminal pairs the span
  // flags legitimately differ (master run-to-return arrival is
  // flag-less) and span symmetry is not structural; everywhere else it
  // still is. Strict counts remain when the clone arrived emulated —
  // the mv attic, DERR, and the :ABORT test keep their enforcement.
  bool terminal_pair = master->terminal && clone->terminal;
  bool span_mismatch = !terminal_pair &&
                       master->native_span != clone->native_span;
  bool count_exempt = terminal_pair
    ? clone->native_span
    : (master->native_span && clone->native_span);

  bool diverged =
    span_mismatch ||
    terminal_mismatch ||
    master->address != clone->address ||
    (master_delta != clone_delta && !count_exempt) ||
    regs_differ ||
    wsp_differs ||
    trap_sites_differ ||
    master_threw != clone_threw ||
    (master_threw && clone_threw &&
     strcmp(master->exception->what(), clone->exception->what()) != 0);

  if(os::Trace::enabled("lockstep")) {
    char buf[160];
    snprintf(buf, sizeof(buf),
             "pair ord=%d pc=%08X insns=%llu clone_pc=%08X clone_insns=%llu%s%s",
             master->machine->lockstep_ordinal,
             master->address, static_cast<unsigned long long>(master_delta),
             clone->address, static_cast<unsigned long long>(clone_delta),
             count_exempt ? " native_span" : "",
             diverged ? " DIVERGED" : "");
    os::Trace::line("lockstep", label_of(master), buf);
  }

  if(!diverged) {
    // Terminal pair: this comparison was the last verified event — the
    // decision to die and the state it dies with.
    if(master->terminal && clone->terminal) {
      if(RTStubs::terminal_kind(master->address)==2) {
        // ABORT kind (DERR.TRP or a :ABORT test point): both engines
        // verifiably agreed the game's own fatal flaw fired here.
        // For DERR the top two stack wides are (number, address) —
        // pushed by the DERR instruction before vectoring.
        char buf[200];
        uint32_t w=static_cast<uint32_t>(master->machine->wsp);
        uint32_t derr_no=master->machine->memory->read_wide(w-1);
        uint32_t derr_at=master->machine->memory->read_wide(w-3);
        snprintf(buf, sizeof(buf),
                 "TERMINAL-ABORT at %08X, verified on both engines "
                 "(top stack wides: %08X %08X — for DERR.TRP: number, faulting pc)",
                 master->address, derr_no, derr_at);
        abort_world(buf, master->machine, /*save=*/false);
        return;
      }
      detach(master, clone);
    }
    return;
  }

  fflush(stdout);
  fprintf(stderr, "LOCKSTEP DIVERGENCE - details in stdout log\n");
  printf("\n================ LOCKSTEP DIVERGENCE ================\n");
  describe("master", master, master);
  describe("clone ", clone, master);
  printf("master backtrace:\n");
  master->machine->backtrace();
  printf("clone backtrace:\n");
  clone->machine->backtrace();
  printf("=====================================================\n");
  fflush(nullptr);
  abort();
}

std::atomic<bool> Lockstep::detached[64] = {};
std::atomic<bool> Lockstep::aborting{false};
std::atomic<bool> Lockstep::suppress_save{false};

void Lockstep::retire_ordinal(Machine* machine) {
  int32_t ordinal = machine->lockstep_ordinal;
  if(ordinal < 0 || ordinal >= 64)
    return;
  bool expected=false;
  static std::atomic<bool> retired[64];
  if(!retired[ordinal].compare_exchange_strong(expected, true))
    return;
  detached[ordinal].store(true, std::memory_order_release);
  os::OSProcess::unmirror_server_mappings();
  clear_copies();
  fprintf(stderr,
          "Lockstep: ordinal %d RETIRED at terminal syscall ?RETURN — "
          "both processes exit independently\n", ordinal);
}

void Lockstep::abort_world(const char* reason, Machine* machine, bool save) {
  bool expected=false;
  if(!aborting.compare_exchange_strong(expected, true))
    return;   // someone already aborting; first reason wins
  if(!save)
    suppress_save.store(true);
  fprintf(stderr, "\n================ WORLD ABORT ================\n%s\n", reason);
  if(machine) {
    fprintf(stderr, "pc=%08X ac0=%08X ac1=%08X ac2=%08X ac3=%08X wsp=%08X wfp=%08X\n",
            (uint32_t)machine->pc, (uint32_t)machine->ac[0], (uint32_t)machine->ac[1],
            (uint32_t)machine->ac[2], (uint32_t)machine->ac[3],
            (uint32_t)machine->wsp, (uint32_t)machine->wfp);
    machine->backtrace();
  }
  fprintf(stderr, "%s\n=============================================\n",
          save ? "Data files will be written back."
               : "Data write-back SUPPRESSED (state presumed corrupt).");
  os::OS::global.shutdown_all();
  os::LockstepMediator::release_all();  // wake mediator-parked tasks (the ctrl-C wedge)
  MachineThread::abort_all();
}

bool Lockstep::is_detached(int32_t ordinal) {
  if(ordinal < 0 || ordinal >= 64)
    return false;
  return detached[ordinal].load(std::memory_order_acquire);
}

void Lockstep::clear_copies() {
  std::lock_guard<std::mutex> lock(copy_registry_mutex);
  copy_registry.clear();
}

// Called from the MachineThread worker at the compare_pair site, where the
// worker holds shared_write_mutex — the same world-pause that
// mirror_server_mappings required going the other direction, so the server
// re-mapping below is safe against concurrent batches and handler writes.
//
// NOTE (multi-client): with a single client pair, retiring the WHOLE copy
// registry is exact. If a second client pair ever exists, registry entries
// need ordinal ownership tags so only the detaching clone's copies retire.
void Lockstep::detach(QueueEntry* master, QueueEntry* clone) {
  int32_t ordinal = master->machine->lockstep_ordinal;
  if(ordinal >= 0 && ordinal < 64) {
    if(detached[ordinal].load())
      return;   // idempotent (e.g. a later terminal on an already-free master)
    detached[ordinal].store(true, std::memory_order_release);
  }

  // Halt every task of the clone's process. Its main task is parked at
  // this batch boundary; it exits its run loop on release. No file writes
  // happen on this path — the master/server pages are the canonical ones
  // saved at shutdown.
  // Since EVERY task halts, EVERY ordinal of this process detaches, not
  // just the terminal one: a secondary task (e.g. the console-interrupt
  // listener) with a batch in flight would otherwise pair its halted
  // remnant against a live master batch — a garbage divergence — or
  // starve the master's ordinal forever. Latent race since the detach
  // machinery landed (timing-dependent: the listener's syscall cadence
  // decides whether a batch straddles the detach); surfaced by the
  // Generation-3 recalibration, where the clone's native terminal chain
  // shifts the detach earlier (docs/Project9/REPORT.md).
  os::OSProcess* clone_process = clone->machine->process;
  if(clone_process) {
    std::lock_guard<std::recursive_mutex> lock(clone_process->task_mutex);
    for(os::OSTask* t : clone_process->tasks)
      if(t) {
        if(t->machine) {
          int32_t o = t->machine->lockstep_ordinal;
          if(o >= 0 && o < 64)
            detached[o].store(true, std::memory_order_release);
        }
        t->halt_task();
      }
  }

  // Revert the server's MirrorPage wrappings to the plain real pages and
  // retire the copy registry (and with it the pair-boundary page audit).
  // The clone stops maintaining its copies the moment it halts, so any
  // further mirroring or compare-on-read would fire false divergences as
  // the master plays on.
  os::OSProcess::unmirror_server_mappings();
  clear_copies();

  std::string where = master->machine->symbols
    ? master->machine->symbols->name_for_address(master->address)
    : std::string("?");
  fprintf(stderr,
          "Lockstep: ordinal %d DETACHED at %08X (%s) — clone halted, "
          "master continues unverified\n",
          ordinal, master->address, where.c_str());
}

void Lockstep::report_waiting(QueueEntry* entry) {
  if(aborting.load())
    return;
  fprintf(stderr,
          "Lockstep: batch waiting for counterpart: label=%s ordinal=%d pc=%08X\n",
          label_of(entry), entry->machine->lockstep_ordinal, entry->address);
}

} // namespace hw
