#include "OSContext.hpp"
#include "ProbeSuppressions.hpp"
#include "OSContextSystem.hpp"
#include "OSContextShared.hpp"
#include "OSContextIPC.hpp"
#include "OSContextFS.hpp"
#include "OSContextTask.hpp"
#include "OSProcess.hpp"
#include "OSTask.hpp"
#include "AOSVSSymbols.hpp"
#include "../hw/Memory.hpp"
#include "../hw/Lockstep.hpp"
#include "../hw/Machine.hpp"
#include <cstdio>
#include <stdexcept>




namespace os {
using namespace hw;

OSContext* OSContext::context_for_call(int32_t call, OSProcess* process, OSTask* task, Memory* memory, Machine* machine) {
  switch(call) {
    case MEM: case MEMI: case GTOD: case RETURN: case PNAME: case DADID:
    case RNGPR: case ERMSG:
    case RECREATE: case IXIT: case INTWT: case WDELAY:
      return new OSContextSystem(process, task, memory, machine);
    case GSHPT: case SSHPT: case SOPEN: case SPAGE: case SCLOSE:
      return new OSContextShared(process, task, memory, machine);
    case CREATE: case SERVE: case ILKUP: case IREC: case ISEND: case ISR:
    case CON: case DCON:
      return new OSContextIPC(process, task, memory, machine);
    case OPEN: case CLOSE: case READ: case WRITE: case UPDATE:
      return new OSContextFS(process, task, memory, machine);
    case TASK: case REC: case KILAD: case DFRSCH: case UIDSTAT:
      return new OSContextTask(process, task, memory, machine);
  }
  char buf[80];
  snprintf(buf, sizeof(buf), "Unimplemented system call %04o", call);
  throw std::runtime_error(buf);
}

OSContext::OSContext(OSProcess* process, OSTask* task, Memory* memory, Machine* machine)
  : process(process), task(task), memory(memory), machine(machine),
    ac0(0), ac1(0), ac2(0), ac3(0) {}

std::string OSContext::full_path(const std::string& name) {
  return task->full_path(name);
}

uint32_t OSContext::aos_error(const std::string& name) {
  auto& map = AOSVSSymbols::symbols();
  auto it = map.find(name);
  if(it == map.end())
    throw std::runtime_error("Undefined AOS/VS symbol '" + name + "'");
  return it->second;
}

uint32_t OSContext::aos_symbol(const std::string& name) {
  return aos_error(name);
}

std::vector<uint8_t> OSContext::read_byte_array(int32_t address, int32_t length) {
  if(length < 0) {
    length = 0;
    while(memory->read_byte(address + length) != 0)
      length++;
  }
  std::vector<uint8_t> bytes(length);
  for(int32_t i = 0; i < length; i++)
    bytes[i] = static_cast<uint8_t>(mem_read_byte(address + i));
  return bytes;
}

std::string OSContext::read_string(int32_t address, int32_t length) {
  auto bytes = read_byte_array(address, length);
  return std::string(bytes.begin(), bytes.end());
}

std::string OSContext::read_string(int32_t address) {
  return read_string(address, -1);
}

// True when a mediated-call read at [byte_lo, byte_hi] should be verified
// against the clone: skip ranges this call already wrote (clone gets them
// via replay) and pages in the mirrored shared region (server mirror
// writes race a two-sided read).
bool OSContext::read_verifiable(uint32_t byte_lo, uint32_t byte_hi,
                                uint32_t word_page) {
  if(write_log) {
    for(const MediatedWrite& w : *write_log) {
      uint32_t lo, hi;
      if(w.width == 1) { lo = w.address; hi = w.address; }
      else if(w.width == 2) { lo = w.address * 2; hi = lo + 1; }
      else { lo = w.address * 2; hi = lo + 3; }
      if(byte_lo <= hi && lo <= byte_hi)
        return false;
    }
  }
  hw::Page* mp = memory->find_page(word_page);
  if(mp && hw::Lockstep::copy_for(mp))
    return false;
  return true;
}

void OSContext::verify_read(uint32_t byte_lo, uint32_t byte_hi,
                            uint64_t mine, uint64_t other, uint32_t width) {
  if(mine == other)
    return;
  // Generation 4 (M4a): a pointer-shaped field (word or byte address into
  // the clone's area/shifted stack) passes when it maps to the master's
  // value — equivalent() (Mapper.md §1.3), the same rule as the register
  // file. mine == other already returned above, so MAPPED is the pass.
  if(read_verify_task && read_verify_task->machine &&
     read_verify_task->machine->equivalent(static_cast<uint32_t>(mine),
                                           static_cast<uint32_t>(other)).kind
       == hw::Mapper::Kind::MAPPED)
    return;
  // Probe mode (Project 11): a differing mediated-call INPUT is the
  // packet-CONTENT specimen tier — flag loudly and continue with the
  // master's (garbage-authentic) value; the clone consumes the master's
  // results, so both engines stay on the master's branch. Never a
  // silent suppression; abort semantics untouched outside probe mode.
  if(hw::Lockstep::probe_relax_regs) {
    ProbeSuppressions::packet_content_flag(byte_lo, byte_hi, width,
                                           mine, other, machine);
    return;
  }
  fflush(stdout);
  fprintf(stderr, "LOCKSTEP DIVERGENCE - details in stdout log\n");
  printf("\n================ LOCKSTEP DIVERGENCE ================\n");
  printf("mediated-call input mismatch (width %u, byte range %06X..%06X):\n",
         width, byte_lo, byte_hi);
  printf("  clone word addr USED at read = %08X\n", last_clone_read_addr);
  printf("  master value = %08llX\n", static_cast<unsigned long long>(mine));
  printf("  clone  value = %08llX\n", static_cast<unsigned long long>(other));
  // Diagnostic context dump (Stage 0b): the word neighbourhood of both
  // sides of the failing read — master at the read address, clone at the
  // clone_location address — so the residue/producer is visible post-mortem.
  {
    uint32_t mw = byte_lo >> 1;
    uint32_t cw = clone_location(mw);
    printf("  read addr: master word=%08X clone word(now)=%08X areas_depth(now)=%zu\n",
           mw, cw,
           read_verify_task && read_verify_task->machine ? read_verify_task->machine->mapping_depth() : 0);
    printf("  master words @%08X:", mw - 8);
    for(int i = -8; i < 8; i++) printf(" %04X", memory->read_word(mw + i));
    printf("\n  clone  words @%08X:", cw - 8);
    for(int i = -8; i < 8; i++) printf(" %04X", read_verify ? read_verify->read_word(cw + i) : 0);
    printf("\n");
  }
  printf("master backtrace:\n");
  if(machine)
    machine->backtrace();
  printf("clone backtrace:\n");
  if(read_verify_task && read_verify_task->machine)
    read_verify_task->machine->backtrace();
  printf("=====================================================\n");
  fflush(nullptr);
  abort();
}

// Generation 4 (M4a): the clone may keep the cell the master reads at a
// stack address in an area frame (or shifted on its real stack);
// verification reads and write replay land at the clone's own storage —
// Mapper clone_location (Mapper.md §1.3), form-aware: the codec
// decomposes word, byte, and @-flagged forms, so callers pass the
// encoded address whole. This thin guard only handles the no-clone case.
uint32_t OSContext::clone_location(uint32_t address) const {
  if(read_verify_task && read_verify_task->machine)
    return read_verify_task->machine->clone_location(address);
  return address;
}

uint32_t OSContext::mem_read_byte(uint32_t address) {
  uint32_t value = memory->read_byte(address);
  if(read_verify && read_verifiable(address, address, address >> 11)) {
    uint32_t caddr = clone_location(address);   // byte form; the codec decomposes
    last_clone_read_addr = caddr;
    verify_read(address, address, value, read_verify->read_byte(caddr), 1);
  }
  return value;
}

uint32_t OSContext::mem_read_word(uint32_t address) {
  uint32_t value = memory->read_word(address);
  if(read_verify && read_verifiable(address * 2, address * 2 + 1,
                                    (address >> 10) & 0x1FFFFF))
    { uint32_t caddr = clone_location(address);
      last_clone_read_addr = caddr;
      verify_read(address * 2, address * 2 + 1, value,
                  read_verify->read_word(caddr), 2); }
  return value;
}

uint32_t OSContext::mem_read_wide(uint32_t address) {
  uint32_t value = memory->read_wide(address);
  if(read_verify && read_verifiable(address * 2, address * 2 + 3,
                                    (address >> 10) & 0x1FFFFF))
    { uint32_t caddr = clone_location(address);
      last_clone_read_addr = caddr;
      verify_read(address * 2, address * 2 + 3, value,
                  read_verify->read_wide(caddr), 4); }
  return value;
}

// True when the master's page at word/byte page number `pn` has a
// registered clone copy — i.e. the address lies in the mirrored shared
// region, where a mediated handler write must reach the clone's copy
// immediately (not at replay time) so a server MirrorPage compare-on-read
// can never observe the real page updated but the copy not. Zero such
// writes have ever been observed (?ISR replies are private packet fields);
// this closes the theoretical window and notes it if it ever occurs.
static bool page_has_clone_copy(hw::Memory* mem, uint32_t page_number) {
  hw::Page* p = mem->find_page(page_number);
  return p != nullptr && hw::Lockstep::copy_for(p) != nullptr;
}

static void note_shared_handler_write(uint32_t address) {
  static bool noted = false;
  if(noted)
    return;
  noted = true;
  fprintf(stderr, "Lockstep: first mediated handler write to a mirrored "
                  "shared-region address (%08X); dual-write applied\n",
          address);
}

void OSContext::mem_write_byte(uint32_t address, uint32_t value) {
  bool dual = write_log && read_verify &&
              page_has_clone_copy(memory, address >> 11);
  if(write_log)
    write_log->push_back({1, address, value, dual});
  if(dual) {
    note_shared_handler_write(address);
    hw::Lockstep::WriteGate gate;   // real + copy as one unit
    memory->write_byte(address, value);
    read_verify->write_byte(clone_location(address), value);   // byte form; codec decomposes
    return;
  }
  memory->write_byte(address, value);
}

void OSContext::mem_write_word(uint32_t address, uint32_t value) {
  bool dual = write_log && read_verify &&
              page_has_clone_copy(memory, (address >> 10) & 0x1FFFFF);
  if(write_log)
    write_log->push_back({2, address, value, dual});
  if(dual) {
    note_shared_handler_write(address);
    hw::Lockstep::WriteGate gate;
    memory->write_word(address, value);
    read_verify->write_word(clone_location(address), value);
    return;
  }
  memory->write_word(address, value);
}

void OSContext::mem_write_wide(uint32_t address, uint32_t value) {
  bool dual = write_log && read_verify &&
              page_has_clone_copy(memory, (address >> 10) & 0x1FFFFF) &&
              page_has_clone_copy(memory, ((address + 1) >> 10) & 0x1FFFFF);
  if(write_log)
    write_log->push_back({4, address, value, dual});
  if(dual) {
    note_shared_handler_write(address);
    hw::Lockstep::WriteGate gate;
    memory->write_wide(address, value);
    read_verify->write_wide(clone_location(address), value);
    return;
  }
  memory->write_wide(address, value);
}

void OSContext::write_byte_array(const std::vector<uint8_t>& bytes, int32_t address, int32_t length) {
  for(int32_t i = 0; i < length; i++)
    mem_write_byte(address + i, bytes[i] & 0xFF);
}

void OSContext::write_byte_array(const std::vector<uint8_t>& bytes, int32_t address) {
  write_byte_array(bytes, address, static_cast<int32_t>(bytes.size()));
}

void OSContext::write_string(const std::string& str, int32_t address) {
  std::vector<uint8_t> bytes(str.begin(), str.end());
  write_byte_array(bytes, address);
}

bool OSContext::read_packet_flag(const std::string& symbol, const std::string& flag, int32_t location) {
  uint32_t offset = aos_symbol(symbol);
  uint32_t mask = aos_symbol(flag);
  return (mask & mem_read_word(location + offset)) != 0;
}

int32_t OSContext::read_packet_byte(const std::string& symbol, int32_t high_low, int32_t location) {
  uint32_t offset = aos_symbol(symbol);
  return static_cast<int32_t>(mem_read_byte((location + offset) * 2 + high_low));
}

int32_t OSContext::read_packet_word(const std::string& symbol, int32_t location) {
  uint32_t offset = aos_symbol(symbol);
  return static_cast<int32_t>(mem_read_word(location + offset));
}

int32_t OSContext::read_packet_wide(const std::string& symbol, int32_t location) {
  uint32_t offset = aos_symbol(symbol);
  return static_cast<int32_t>(mem_read_wide(location + offset));
}

bool OSContext::read_packet_flag(const std::string& symbol, const std::string& flag) {
  return read_packet_flag(symbol, flag, ac2);
}

int32_t OSContext::read_packet_byte(const std::string& symbol, int32_t high_low) {
  return read_packet_byte(symbol, high_low, ac2);
}

int32_t OSContext::read_packet_word(const std::string& symbol) {
  return read_packet_word(symbol, ac2);
}

int32_t OSContext::read_packet_wide(const std::string& symbol) {
  return read_packet_wide(symbol, ac2);
}

void OSContext::write_packet_byte(const std::string& symbol, int32_t high_low, int32_t value, int32_t location) {
  uint32_t offset = aos_symbol(symbol);
  mem_write_byte((location + offset) * 2 + high_low, value);
}

void OSContext::write_packet_word(const std::string& symbol, int32_t value, int32_t location) {
  uint32_t offset = aos_symbol(symbol);
  mem_write_word(location + offset, value);
}

void OSContext::write_packet_wide(const std::string& symbol, int32_t value, int32_t location) {
  uint32_t offset = aos_symbol(symbol);
  mem_write_wide(location + offset, value);
}

void OSContext::write_packet_byte(const std::string& symbol, int32_t high_low, int32_t value) {
  write_packet_byte(symbol, high_low, value, ac2);
}

void OSContext::write_packet_word(const std::string& symbol, int32_t value) {
  write_packet_word(symbol, value, ac2);
}

void OSContext::write_packet_wide(const std::string& symbol, int32_t value) {
  write_packet_wide(symbol, value, ac2);
}

int32_t OSContext::dispatch_system_call(int32_t call) {
  throw std::runtime_error("Dispatch system call must be implemented by the subclass");
}

} // namespace os
