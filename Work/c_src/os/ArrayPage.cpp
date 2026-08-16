#include "ArrayPage.hpp"
#include "Trace.hpp"
#include "OSProcess.hpp"
#include "../hw/MachineThread.hpp"
#include "../hw/Lockstep.hpp"
#include "../hw/Machine.hpp"
#include <cstdio>

namespace os {
static void trace_write(const std::string& label, const char* op,
                        uint32_t offset, uint32_t value) {
  if(label.empty() || !Trace::enabled("shared"))
    return;
  const char* caller = "?";
  bool handler = false;
  if(!hw::Lockstep::on_worker_thread && hw::Lockstep::task_thread_label) {
    caller = hw::Lockstep::task_thread_label;   // syscall handler write
    handler = true;
  }
  else {
    hw::Machine* m = hw::MachineThread::current_machine;
    if(m != nullptr && m->process != nullptr)
      caller = m->process->instance_label.c_str();
  }
  char buf[112];
  snprintf(buf, sizeof(buf), "page=%s op=%s off=0x%03X val=0x%08X%s",
           label.c_str(), op, offset, value, handler ? " (handler)" : "");
  Trace::line("shared", caller, buf);
}


} // namespace os




namespace os {
ArrayPage::ArrayPage()
  : bytes(2048, 0) {}

ArrayPage::ArrayPage(const std::vector<uint8_t>& bytes)
  : bytes(bytes) {
  this->bytes.resize(2048, 0);
}

ArrayPage::ArrayPage(const std::vector<uint8_t>& bytes, std::function<void()> on_modified)
  : bytes(bytes), on_modified(on_modified) {
  this->bytes.resize(2048, 0);
}

uint32_t ArrayPage::read(uint32_t offset) {
  return static_cast<uint32_t>(bytes[offset]) & 0xFF;
}

void ArrayPage::write(uint32_t offset, uint32_t value) {
  bytes[offset] = static_cast<uint8_t>(value & 0xFF);
}

void ArrayPage::write_byte(uint32_t offset, uint32_t value) {
  hw::Lockstep::WriteGate gate;
  Page::write_byte(offset, value);
  trace_write(trace_label, "byte", offset, value & 0xFF);
  if(on_modified) on_modified();
}

void ArrayPage::write_word(uint32_t offset, uint32_t value) {
  hw::Lockstep::WriteGate gate;
  Page::write_word(offset, value);
  trace_write(trace_label, "word", offset, value & 0xFFFF);
  if(on_modified) on_modified();
}

void ArrayPage::write_wide(uint32_t offset, uint32_t value) {
  hw::Lockstep::WriteGate gate;
  Page::write_wide(offset, value);
  trace_write(trace_label, "wide", offset, value);
  if(on_modified) on_modified();
}

} // namespace os
