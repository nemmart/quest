#include "MirrorPage.hpp"
#include "ArrayPage.hpp"
#include "../hw/Lockstep.hpp"
#include "../hw/Machine.hpp"
#include "../hw/MachineThread.hpp"
#include "OSProcess.hpp"
#include <cstdio>
#include <cstdlib>


namespace os {
using hw::Lockstep;

// Compare-on-read: every server read of a mirrored shared page reads both
// the real page and the clone's copy; the real value is USED, the copy is
// COMPARED. A mismatch means the clone's shared-page state has drifted
// from the master's — reported at the moment the server would consume the
// data. Off-worker reads (server syscall handlers) take the WriteGate so
// the two-sided read cannot interleave a client pair (between the master
// and clone halves the pages legally differ); worker-side reads are
// already serialized by the per-batch shared_write_mutex hold.

static void report_mismatch(hw::Page* real, const char* op, uint32_t offset,
                            uint32_t real_value, uint32_t copy_value) {
  fflush(stdout);
  fprintf(stderr, "LOCKSTEP DIVERGENCE - details in stdout log\n");
  printf("\n================ LOCKSTEP DIVERGENCE ================\n");
  const char* label = "?";
  if(auto* ap = dynamic_cast<ArrayPage*>(real))
    if(!ap->trace_label.empty())
      label = ap->trace_label.c_str();
  printf("MirrorPage read mismatch (server consuming shared data):\n");
  printf("  page=%s op=%s off=0x%03X real=%08X clone_copy=%08X\n",
         label, op, offset, real_value, copy_value);
  if(Lockstep::on_worker_thread && hw::MachineThread::current_machine) {
    hw::Machine* m = hw::MachineThread::current_machine;
    printf("reader: %s (instruction)\n",
           m->process ? m->process->instance_label.c_str() : "?");
    printf("reader backtrace:\n");
    m->backtrace();
  }
  else {
    printf("reader: %s (handler)\n",
           Lockstep::task_thread_label ? Lockstep::task_thread_label : "?");
  }
  printf("(writer provenance: run with -trace FILE -types shared)\n");
  printf("=====================================================\n");
  fflush(nullptr);
  abort();
}

uint32_t MirrorPage::read(uint32_t offset) {
  Lockstep::WriteGate gate;
  uint32_t r = real->read(offset);
  uint32_t c = mirror->read(offset);
  if(r != c)
    report_mismatch(real, "raw", offset, r, c);
  return r;
}

void MirrorPage::write(uint32_t offset, uint32_t value) {
  Lockstep::WriteGate gate;
  real->write(offset, value);
  mirror->write(offset, value);
}

uint32_t MirrorPage::read_byte(uint32_t offset) {
  Lockstep::WriteGate gate;
  uint32_t r = real->read_byte(offset);
  uint32_t c = mirror->read_byte(offset);
  if(r != c)
    report_mismatch(real, "byte", offset, r, c);
  return r;
}

uint32_t MirrorPage::read_word(uint32_t offset) {
  Lockstep::WriteGate gate;
  uint32_t r = real->read_word(offset);
  uint32_t c = mirror->read_word(offset);
  if(r != c)
    report_mismatch(real, "word", offset, r, c);
  return r;
}

uint32_t MirrorPage::read_wide(uint32_t offset) {
  Lockstep::WriteGate gate;
  uint32_t r = real->read_wide(offset);
  uint32_t c = mirror->read_wide(offset);
  if(r != c)
    report_mismatch(real, "wide", offset, r, c);
  return r;
}

void MirrorPage::write_byte(uint32_t offset, uint32_t value) {
  Lockstep::WriteGate gate;   // hold across both so a pair sees them together
  real->write_byte(offset, value);
  mirror->write_byte(offset, value);
}

void MirrorPage::write_word(uint32_t offset, uint32_t value) {
  Lockstep::WriteGate gate;
  real->write_word(offset, value);
  mirror->write_word(offset, value);
}

void MirrorPage::write_wide(uint32_t offset, uint32_t value) {
  Lockstep::WriteGate gate;
  real->write_wide(offset, value);
  mirror->write_wide(offset, value);
}

} // namespace os
