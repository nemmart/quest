#include "OSProcess.hpp"
#include "../hw/AddressBook.hpp"
#include "../hw/RTStubs.hpp"
#include "MirrorPage.hpp"
#include "ArrayPage.hpp"
#include "../hw/Lockstep.hpp"
#include "../hw/Permissions.hpp"
#include "OS.hpp"
#include "OSTask.hpp"
#include "OSError.hpp"
#include "FS.hpp"
#include "FSObject.hpp"
#include "FSFile.hpp"
#include "FSChannel.hpp"
#include "FSStreamIO.hpp"
#include "ArrayPage.hpp"
#include "../hw/Memory.hpp"
#include "../hw/Machine.hpp"
#include "../hw/PageSet.hpp"
#include "../hw/Page.hpp"
#include "../hw/Permissions.hpp"
#include "../debug/SymbolTable.hpp"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>




namespace os {
using namespace debug;
using namespace hw;

std::string OSProcess::full_path(const std::string& wd, const std::string& filename) {
  if(!filename.empty() && filename[0] == ':') return filename;
  if(!filename.empty() && filename[0] == '@') return filename;
  if(wd == ":") return ":" + filename;
  return wd + ":" + filename;
}

OSProcess::OSProcess(const std::string& wd, const std::string& prog)
  : pid(-1), program(prog), working_directory(wd),
    symbols(nullptr), memory(nullptr),
    unshared_stop(0), shared_start(0),
    terminating(false), system_call_logging(true),
    page0(nullptr), page8(nullptr)
{
  std::string fp = full_path(wd, prog);
  FSObject* symbol_file;
  FSChannel* channel = nullptr;
  PageSet* page_set;

  // Load symbol table
  symbol_file = FS::retrieve(fp + ".ST");
  if(symbol_file) {
    FSFile* sf = dynamic_cast<FSFile*>(symbol_file);
    if(sf) symbols = new SymbolTable(SymbolTable::parse_st_format(sf->full_contents()));
  }
  if(!symbols) {
    symbol_file = FS::retrieve(fp + ".SYM");
    if(symbol_file) {
      FSFile* sf = dynamic_cast<FSFile*>(symbol_file);
      if(sf) symbols = new SymbolTable(SymbolTable::parse_sym_format(sf->full_contents()));
    }
  }
  if(!symbols)
    symbols = new SymbolTable();

  // Load program file
  try {
    channel = FSChannel::open_for_paged_io(fp + ".PR", true);
    page_set = channel->page_set();
    page0 = page_set->find_page(0);
    page8 = page_set->find_page(8);
  }
  catch(OSError& error) {
    throw std::runtime_error("Unable to load " + fp + ".PR");
  }

  int32_t shared_start_page = static_cast<int32_t>(page0->read_word(0x10F));
  int32_t shared_page_count = static_cast<int32_t>(page0->read_word(0x113));
  int32_t file_shared_offset = static_cast<int32_t>(page0->read_word(0x11A));

  shared_pages.resize(shared_page_count, nullptr);
  unshared_stop = file_shared_offset - 8;
  shared_start = shared_start_page;

  memory = new Memory();
  memory->process_name = program;
  map_file(channel, SEGMENT_BASE, file_shared_offset - 8, 8, false, true, true);
  map_file(channel, shared_start_page + SEGMENT_BASE, shared_page_count, file_shared_offset, true, false, true);

  // Native registration happens in launch() — lockstep_role is assigned by
  // Launch after construction, and stubs/translations are clone-only.

  // Don't delete channel — pages reference its PageSet's Page objects
}

OSProcess::~OSProcess() {
  delete symbols;
  delete memory;
}

void OSProcess::launch(FSStreamIO* terminal) {
  std::string fp = full_path(working_directory, program);

  tasks.resize(32, nullptr);
  channels.resize(128, nullptr);
  channels[0] = FSChannel::open_for_streamed_io(terminal, FSChannel::READ_WRITE_PERMISSION);
  channels[1] = FSChannel::open_for_paged_io(fp + ".PR", true);

  terminating = false;
  pid = OS::global.register_process(this);
  system_call_logging = true;

  // RT range/coverage/sync setup (QUEST only; idempotent), then native
  // registration in the clone only: master and non-lockstep runs stay
  // pure emulation.
  hw::RTStubs::initialize(*symbols, program);
  if(lockstep_role == hw::Lockstep::CLONE)
    native_registry.register_all(*symbols, program);
  // M4a: the address-book areas — mapped at launch, exactly the book's
  // pages, RW/no-exec, on the clone (or a non-lockstep QUEST client).
  if(hw::AddressBook::active() &&
     (lockstep_role == hw::Lockstep::CLONE || (!hw::Lockstep::enabled && program == "QUEST"))) {
    hw::AddressBook::instance->map_pages(*memory);
    mapper_book = hw::AddressBook::instance;   // this process's machines redirect
  }
  // Fault injector arming: QUEST clients only (both lockstep roles and
  // single-machine runs; never QUEST_SERVER). One shot per process.
  {
    std::string upper=program;
    for(char& ch : upper) ch=toupper(ch);
    inject_armed = (upper=="QUEST" && hw::RTStubs::inject_site!=0);
    poke_armed   = (upper=="QUEST" && hw::RTStubs::poke_pc!=0);
  }

  int32_t start_addr = static_cast<int32_t>(page0->read_wide(0x17C));
  int32_t wfp  = static_cast<int32_t>(page8->read_wide(16));
  int32_t wsp  = static_cast<int32_t>(page8->read_wide(18));
  int32_t wsb  = static_cast<int32_t>(page8->read_wide(20));
  int32_t wsl  = static_cast<int32_t>(page8->read_wide(22));
  int32_t sfh  = static_cast<int32_t>(page8->read_word(12));

  OSTask* task = new OSTask(this, start_addr, wfp, wsp, wsb, wsl, sfh);
  register_task(task);
  task->launch();
}

FSStreamIO* OSProcess::console() {
  // Channel 0 is always the streaming terminal/console
  return channels[0]->stream_io;
}

Page* OSProcess::private_copy(Page* page) {
  auto* ap = new ArrayPage();
  for(int i = 0; i < 2048; i++)
    ap->bytes[i] = static_cast<uint8_t>(page->read_byte(i));
  return ap;
}

void OSProcess::map_file(FSChannel* channel, int32_t memory_page, int32_t pc,
                         int32_t file_page, bool shared, bool write_perm, bool exec_perm) {
  PageSet* ps = channel->page_set();
  int32_t permissions = 0;

  if(!shared)
    permissions = Permissions::PERMISSIONS_READ_WRITE_EXECUTE;
  else if(shared && !write_perm && exec_perm)
    permissions = Permissions::PERMISSIONS_READ_EXECUTE;
  else if(shared && !write_perm && !exec_perm)
    permissions = Permissions::PERMISSION_READ;
  else
    permissions = Permissions::PERMISSIONS_READ_WRITE;

  for(int32_t i = 0; i < pc; i++) {
    if(shared)
      memory->map_page(lockstep_shared_page(ps, file_page + i, write_perm, exec_perm),
                       memory_page + i, permissions);
    else
      memory->map_page(private_copy(ps->find_page(file_page + i)), memory_page + i, permissions);
    if(shared && !exec_perm)
      shared_pages[memory_page + i - shared_start - SEGMENT_BASE] =
        new OSSharedPageSource(channel, file_page + i);
  }
}

// Lockstep step 3 (docs/LockstepHarness.md): resolve which page object a
// shared mapping should use.
//   MASTER  -> the real file page (shared with the server), as always.
//   CLONE   -> a private copy of each writable shared data page, snapshotted
//              and registered under a world-pause (shared_write_mutex holds
//              off server batches and handler writes), then the server's
//              existing mappings of that page are rewrapped in MirrorPages
//              so its future writes land in both.
//   SERVER  -> a MirrorPage if a clone copy already exists (late mapping).
// Shared code (exec) and read-only mappings stay physically shared.
Page* OSProcess::lockstep_shared_page(PageSet* ps, int32_t file_page,
                                          bool write_perm, bool exec_perm) {
  hw::Page* real = ps->find_page(file_page);
  if(!hw::Lockstep::enabled || exec_perm || !write_perm)
    return real;

  if(lockstep_role == hw::Lockstep::CLONE) {
    hw::Lockstep::WriteGate pause;   // stop server batches + handler writes
    hw::Page* existing = hw::Lockstep::copy_for(real);
    if(existing)
      return existing;               // e.g. re-mapping after ?RECREATE
    hw::Page* copy = private_copy(real);
    if(auto* rap = dynamic_cast<ArrayPage*>(real)) {
      auto* cap = static_cast<ArrayPage*>(copy);
      if(!rap->trace_label.empty())
        cap->trace_label = rap->trace_label + "~clone";
    }
    hw::Lockstep::register_copy(real, copy);
    mirror_server_mappings(real, copy);
    return copy;
  }

  if(lockstep_role == hw::Lockstep::SERVER) {
    if(hw::Page* copy = hw::Lockstep::copy_for(real))
      return new MirrorPage(real, copy);
  }
  return real;
}

// Rewrap every SERVER-role process's live mapping of `real` with a
// MirrorPage(real, copy). Uses the shared_pages bookkeeping to find the
// mapped locations without scanning whole address spaces. Called under the
// world-pause, so the worker cannot be dereferencing these page pointers.
void OSProcess::mirror_server_mappings(Page* real, Page* copy) {
  for(auto& entry : OS::global.pids) {
    OSProcess* p = entry.second;
    if(!p || p->lockstep_role != hw::Lockstep::SERVER)
      continue;
    for(int32_t s = 0; s < static_cast<int32_t>(p->shared_pages.size()); s++) {
      OSSharedPageSource* src = p->shared_pages[s];
      if(!src)
        continue;
      if(src->channel->page_set()->find_page(src->page_number) != real)
        continue;
      uint32_t page_number = static_cast<uint32_t>(s + p->shared_start + SEGMENT_BASE);
      p->memory->map_page(new MirrorPage(real, copy), page_number,
                          hw::Permissions::PERMISSIONS_READ_WRITE);
    }
  }
}

// Walk every SERVER process's shared mappings; wherever the underlying
// file page has a registered clone copy (i.e. the mapping was wrapped in a
// MirrorPage), map the plain real page back. After this the server writes
// and reads only the canonical pages; the frozen clone copies are inert
// until freed at teardown. Caller holds shared_write_mutex.
void OSProcess::unmirror_server_mappings() {
  for(auto& entry : OS::global.pids) {
    OSProcess* p = entry.second;
    if(!p || p->lockstep_role != hw::Lockstep::SERVER)
      continue;
    for(int32_t s = 0; s < static_cast<int32_t>(p->shared_pages.size()); s++) {
      OSSharedPageSource* src = p->shared_pages[s];
      if(!src)
        continue;
      hw::Page* real = src->channel->page_set()->find_page(src->page_number);
      if(!real || !hw::Lockstep::copy_for(real))
        continue;
      uint32_t page_number = static_cast<uint32_t>(s + p->shared_start + SEGMENT_BASE);
      p->memory->map_page(real, page_number,
                          hw::Permissions::PERMISSIONS_READ_WRITE);
    }
  }
}

int32_t OSProcess::count_tasks() {
  std::lock_guard<std::recursive_mutex> lock(task_mutex);
  int32_t count = 0;
  for(auto* t : tasks)
    if(t) count++;
  return count;
}

int32_t OSProcess::task_slot(OSTask* task) {
  std::lock_guard<std::recursive_mutex> lock(task_mutex);
  for(int32_t i = 0; i < static_cast<int32_t>(tasks.size()); i++)
    if(tasks[i] == task) return i;
  return -1;
}

bool OSProcess::register_task(OSTask* task) {
  std::lock_guard<std::recursive_mutex> lock(task_mutex);
  int32_t tid = next_tid++;
  if(terminating) {
    task->tid = -1;
    return false;
  }
  for(int32_t i = 0; i < static_cast<int32_t>(tasks.size()); i++) {
    if(!tasks[i]) {
      task->tid = tid;
      tasks[i] = task;
      // Lockstep metadata: master/clone batches pair by task creation
      // ordinal within each process (docs/LockstepHarness.md).
      if(task->machine) {
        task->machine->lockstep_role = lockstep_role;
        task->machine->lockstep_ordinal = next_lockstep_ordinal;
        // Ruling 8 garbage probe (-zero=clone): master runs UNZEROED,
        // clone runs ZEROED — the asymmetric configuration IS the
        // experiment (docs/Project10). BOTH/NONE were set uniformly at
        // machine construction; only the clone-keyed case needs the role.
        if(hw::Machine::zero_mode == hw::Machine::ZERO_CLONE)
          task->machine->zero_claims = (lockstep_role == hw::Lockstep::CLONE);
      }
      next_lockstep_ordinal++;
      return true;
    }
  }
  throw std::runtime_error("Register task: too many tasks are running");
}

void OSProcess::unregister_task(OSTask* task) {
  bool last = false;
  {
    std::lock_guard<std::recursive_mutex> lock(task_mutex);
    task->tid = -1;
    for(int32_t i = 0; i < static_cast<int32_t>(tasks.size()); i++) {
      if(tasks[i] == task) {
        tasks[i] = nullptr;
        break;
      }
    }
    if(count_tasks() == 0) {
      OS::global.unregister_process(this);
      pid = -1;
      last = true;
    }
  }
  if(last) {
    for(int32_t i = 0; i < static_cast<int32_t>(channels.size()); i++) {
      if(channels[i]) {
        FSFile* file = channels[i]->get_file();
        if(file)
          fprintf(stderr, "OPEN Channel %d, path %s modified: %d\n",
                  i, file->get_path().c_str(), file->modified);
      }
    }
  }
}

int32_t OSProcess::next_channel() {
  for(int32_t i = 0; i < static_cast<int32_t>(channels.size()); i++)
    if(!channels[i]) return i;
  return -1;
}

int32_t OSProcess::assign_channel(FSChannel* channel) {
  for(int32_t i = 0; i < static_cast<int32_t>(channels.size()); i++) {
    if(!channels[i]) {
      channels[i] = channel;
      return i;
    }
  }
  throw OSError(OSError::OS_MAXIMUM_NUMBER_OF_FILES_OPEN);
}

FSChannel* OSProcess::retrieve_channel(int32_t ch) {
  if(ch < 0 || ch > 127)
    throw OSError(OSError::OS_INVALID_CHANNEL_NUMBER);
  if(!channels[ch])
    throw OSError(OSError::OS_INVALID_CHANNEL_NUMBER);
  return channels[ch];
}

} // namespace os
