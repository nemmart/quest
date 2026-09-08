// src/emu_types/OperatingSystem.cpp
#include "OperatingSystem.hpp"
#include "../emu_quest/SharedData.hpp"
#include "SharedRandomState.hpp"
#include "../hw/Machine.hpp"
#include "../os/OSProcess.hpp"
#include "../os/OSError.hpp"
#include "../os/OS.hpp"
#include "../os/OSSharedPageSource.hpp"
#include "../os/OSMessage.hpp"
#include "../os/FSChannel.hpp"
#include "../os/FSFile.hpp"
#include "../types/String.hpp"
#include "../types/StdString.hpp"
#include "../types/WordArray.hpp"
#include "../types/ByteArray.hpp"
#include "../os/OSTask.hpp"
#include <mutex>
#include <vector>
#include <chrono>
#include <thread>
#include <ctime>
#include <cstdio>
#include <stdexcept>

namespace emu_types {

OperatingSystem::OperatingSystem(hw::Machine& m)
  : machine(m), owned_shared_data(nullptr), owned_random_state(nullptr) {}

OperatingSystem::~OperatingSystem() {
  delete owned_shared_data;
  delete owned_random_state;
}

// Global variable addresses
static constexpr uint32_t SD_PTR_ADDR=0x70000210;
static constexpr uint32_t OBJ_PTR_ADDR=0x70000212;
static constexpr uint32_t CAS_PTR_ADDR=0x70000214;
static constexpr uint32_t SH_CHAN1_ADDR=0x70000264;
static constexpr uint32_t SH_CHAN2_ADDR=0x70000266;
static constexpr uint32_t SH_CHAN3_ADDR=0x70000268;
static constexpr uint32_t OBJ_BASE=0x70017C00;
static constexpr uint32_t CAS_BASE=0x7012AC00;
static constexpr int32_t SD_PAGES=150;
static constexpr int32_t OBJ_PAGES=1100;
static constexpr int32_t CAS_PAGES=150;

quest::SharedData* OperatingSystem::init_shared_data() {
  // Step 1: Compute SD_PTR from current shared page table.
  // Extract segment bits from the stack pointer, then place SD_PTR
  // at the word address of the current shared page boundary.
  uint32_t segment=static_cast<uint32_t>(machine.wsp)&0x70000000;

  int32_t shared_start=0;
  int32_t page_count=0;
  gshpt(shared_start, page_count);

  int32_t end=shared_start+page_count;
  if(end<0) end=-end;
  uint32_t sd_ptr=segment|static_cast<uint32_t>(end<<10);
  machine.memory->write_wide(SD_PTR_ADDR, sd_ptr);

  // Step 2: Extend shared page table by SD_PAGES.
  int32_t new_page_count=page_count+SD_PAGES;
  int32_t err=sshpt(shared_start, new_page_count);
  if(err)
    throw std::runtime_error("INIT_SHARED_DATA: cannot allocate shared partition");

  // Step 3: Open the three shared memory files.
  types::StdString shared_name("SHARED_DATA_FILE");
  types::StdString world_name("WORLD_DATA_FILE");
  types::StdString castle_name("CASTLE_DATA_FILE");

  int32_t sh_chan1=0, sh_chan2=0, sh_chan3=0;

  err=open_shared_io_file(shared_name, 0, sh_chan1);
  if(err)
    throw std::runtime_error("INIT_SHARED_DATA: cannot open SHARED_DATA_FILE");
  machine.memory->write_word(SH_CHAN1_ADDR, static_cast<uint32_t>(sh_chan1));

  err=open_shared_io_file(world_name, 0, sh_chan2);
  if(err)
    throw std::runtime_error("INIT_SHARED_DATA: cannot open WORLD_DATA_FILE");
  machine.memory->write_word(SH_CHAN2_ADDR, static_cast<uint32_t>(sh_chan2));

  err=open_shared_io_file(castle_name, 0, sh_chan3);
  if(err)
    throw std::runtime_error("INIT_SHARED_DATA: cannot open CASTLE_DATA_FILE");
  machine.memory->write_word(SH_CHAN3_ADDR, static_cast<uint32_t>(sh_chan3));

  // Step 4: Map shared pages into memory.
  err=get_shared_page(sh_chan1, static_cast<int32_t>(sd_ptr), SD_PAGES, 0);
  if(err)
    throw std::runtime_error("INIT_SHARED_DATA: cannot map SHARED_DATA_FILE");

  machine.memory->write_wide(OBJ_PTR_ADDR, OBJ_BASE);

  err=get_shared_page(sh_chan2, static_cast<int32_t>(OBJ_BASE), OBJ_PAGES, 0);
  if(err)
    throw std::runtime_error("INIT_SHARED_DATA: cannot map WORLD_DATA_FILE");

  machine.memory->write_wide(CAS_PTR_ADDR, CAS_BASE);

  err=get_shared_page(sh_chan3, static_cast<int32_t>(CAS_BASE), CAS_PAGES, 0);
  if(err)
    throw std::runtime_error("INIT_SHARED_DATA: cannot map CASTLE_DATA_FILE");

  // Step 5: Create SharedData accessor for the mapped regions.
  owned_shared_data=new emu_quest::SharedData(*machine.memory);

  // Step 6: Create SharedRandomState for the random seed in shared memory.
  // The seed lives at SD_PTR + 0x28 (global field, not per-player).
  static constexpr uint32_t SEED_OFFSET=0x28;
  owned_random_state=new emu_types::SharedRandomState(*machine.memory, sd_ptr+SEED_OFFSET);

  return owned_shared_data;
}

types::SharedRandomState* OperatingSystem::get_random_state() {
  return owned_random_state;
}

int32_t OperatingSystem::current_pid(int32_t& pid) {
  pid=machine.process->pid;
  return 0;
}

int32_t OperatingSystem::connect(int32_t pid) {
  try {
    os::OS::global.connect(machine.process, pid);
    return 0;
  } catch(os::OSError& e) {
    return e.error();
  }
}

int32_t OperatingSystem::disconnect(int32_t pid) {
  // DCON is a no-op in the emulator
  return 0;
}

int32_t OperatingSystem::serve(int32_t port) {
  // SERVE is a no-op in the emulator
  return 0;
}

void OperatingSystem::terminate_process(const std::string& message) {
  fprintf(stderr, "TERMINATING PROCESS\n");
  if(!message.empty())
    fprintf(stderr, "   message = %s\n", message.c_str());
  else
    fprintf(stderr, "   no termination message\n");
  machine.process->terminating=true;
  {
    std::lock_guard<std::recursive_mutex> lock(machine.process->task_mutex);
    for(size_t i=0; i<machine.process->tasks.size(); i++) {
      if(machine.process->tasks[i] && machine.process->tasks[i]!=machine.task)
        machine.process->tasks[i]->halt_task();
    }
  }
  while(machine.process->count_tasks()>1) {
    fprintf(stderr, "Waiting for tasks to terminate\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  throw std::runtime_error("EXIT!");
}

int32_t OperatingSystem::lookup_port(const types::String& service_name, int32_t& port) {
  std::string sname=service_name.str();
  size_t nul=sname.find('\0');
  if(nul!=std::string::npos)
    sname.resize(nul);
  try {
    port=os::OS::global.retrieve_service(sname);
    return 0;
  } catch(os::OSError& e) {
    return e.error();
  }
}

int32_t OperatingSystem::ipc_send(int32_t destination_port,
                                  int32_t origin_port,
                                  int32_t user_flags,
                                  const types::WordArray& send_data,
                                  int32_t value) {
  try {
    int32_t origin=(machine.process->pid<<16)|origin_port;
    std::vector<int32_t> content=send_data.to_vector();
    os::OSMessage msg(origin, destination_port, user_flags, value, content);
    os::OS::global.send_message(msg);
    return 0;
  } catch(os::OSError& e) {
    return e.error();
  }
}

int32_t OperatingSystem::ipc_receive(int32_t& origin,
                                     int32_t& destination_port,
                                     int32_t& user_flags,
                                     types::WordArray& receive_data,
                                     int32_t& value) {
  try {
    os::OSMessage* msg=os::OS::global.receive_message(machine.process->pid);
    if(!msg)
      return -1;  // interrupted

    size_t rcv_len=msg->content.size();
    if(rcv_len>receive_data.capacity())
      rcv_len=receive_data.capacity();
    receive_data.set_size(rcv_len);
    for(size_t i=0; i<rcv_len; i++)
      receive_data.set_word(i, msg->content[i]);

    origin=msg->origin();
    destination_port=msg->destination_port;
    user_flags=msg->user_flags;
    if(msg->content.empty())
      value=msg->pointer;

    delete msg;
    return 0;
  } catch(os::OSError& e) {
    return e.error();
  }
}

int32_t OperatingSystem::ipc_send_receive(int32_t destination_port,
                                          int32_t origin_port,
                                          int32_t& user_flags,
                                          const types::WordArray& send_data,
                                          int32_t send_value,
                                          types::WordArray& receive_data,
                                          int32_t& receive_value) {
  // Send
  int32_t err=ipc_send(destination_port, origin_port, user_flags, send_data, send_value);
  if(err) return err;

  // Receive — reuse origin/destination as throwaway locals
  int32_t reply_origin, reply_dest;
  return ipc_receive(reply_origin, reply_dest, user_flags, receive_data, receive_value);
}

int32_t OperatingSystem::write_screen(int32_t channel, const types::String& text) {
  try {
    os::FSChannel* ch;
    {
      std::lock_guard<std::mutex> lock(machine.process->channel_mutex);
      ch=machine.process->retrieve_channel(channel);
    }
    std::string s=text.str();
    std::vector<uint8_t> bytes(s.begin(), s.end());
    ch->write(bytes);
    return 0;
  } catch(os::OSError& e) {
    return e.error();
  }
}

int32_t OperatingSystem::write_screen(int32_t channel, const types::String& text,
                                      int row, int col, int32_t options) {
  static constexpr int32_t ESCP=0x0800;
  try {
    os::FSChannel* ch;
    {
      std::lock_guard<std::mutex> lock(machine.process->channel_mutex);
      ch=machine.process->retrieve_channel(channel);
    }
    if(options&ESCP) {
      std::vector<uint8_t> cursor={0x10,
        static_cast<uint8_t>(col&0xFF),
        static_cast<uint8_t>(row&0xFF)};
      ch->write(cursor);
    }
    std::string s=text.str();
    std::vector<uint8_t> bytes(s.begin(), s.end());
    ch->write(bytes);
    return 0;
  } catch(os::OSError& e) {
    return e.error();
  }
}

int32_t OperatingSystem::read_screen(int32_t channel, types::String& result,
                                     int32_t max_length) {
  try {
    os::FSChannel* ch;
    {
      std::lock_guard<std::mutex> lock(machine.process->channel_mutex);
      ch=machine.process->retrieve_channel(channel);
    }
    std::vector<uint8_t> bytes(static_cast<size_t>(max_length));
    int32_t amount=ch->read(bytes, true);
    if(amount<0) amount=0;
    result.assign(std::string(bytes.begin(), bytes.begin()+amount));
    return 0;
  } catch(os::OSError& e) {
    return e.error();
  }
}

int32_t OperatingSystem::read_channel(int32_t channel, types::ByteArray& buffer,
                                      int32_t options, bool& eof) {
  static constexpr int32_t IBIN=0x1000;
  try {
    os::FSChannel* ch;
    {
      std::lock_guard<std::mutex> lock(machine.process->channel_mutex);
      ch=machine.process->retrieve_channel(channel);
    }
    bool line_mode=!(options&IBIN);
    std::vector<uint8_t> bytes(buffer.capacity());
    int32_t amount=ch->read(bytes, line_mode);
    if(amount==-1) {
      eof=true;
      buffer.set_size(0);
    }
    else {
      eof=false;
      buffer.assign(bytes.data(), static_cast<size_t>(amount));
    }
    return 0;
  } catch(os::OSError& e) {
    return e.error();
  }
}

int32_t OperatingSystem::read_channel_at(int32_t channel, types::ByteArray& buffer,
                                         int32_t options, int32_t record_number,
                                         bool& eof) {
  static constexpr int32_t IBIN=0x1000;
  try {
    os::FSChannel* ch;
    {
      std::lock_guard<std::mutex> lock(machine.process->channel_mutex);
      ch=machine.process->retrieve_channel(channel);
    }
    ch->set_position(record_number);
    bool line_mode=!(options&IBIN);
    std::vector<uint8_t> bytes(buffer.capacity());
    int32_t amount=ch->read(bytes, line_mode);
    if(amount==-1) {
      eof=true;
      buffer.set_size(0);
    }
    else {
      eof=false;
      buffer.assign(bytes.data(), static_cast<size_t>(amount));
    }
    return 0;
  } catch(os::OSError& e) {
    return e.error();
  }
}

int32_t OperatingSystem::open_file(const types::String& filename, int32_t options,
                                   int32_t& channel) {
  try {
    std::string name=filename.str();
    size_t nul=name.find('\0');
    if(nul!=std::string::npos)
      name.resize(nul);
    std::string path=machine.task->full_path(name);
    os::FSChannel* ch=os::OS::global.open_file(machine.process, path, options);
    std::lock_guard<std::mutex> lock(machine.process->channel_mutex);
    channel=machine.process->assign_channel(ch);
    return 0;
  } catch(os::OSError& e) {
    return e.error();
  }
}

int32_t OperatingSystem::close_file(int32_t channel) {
  try {
    std::lock_guard<std::mutex> lock(machine.process->channel_mutex);
    os::FSChannel* ch=machine.process->retrieve_channel(channel);
    ch->close();
    machine.process->channels[channel]=nullptr;
    return 0;
  } catch(os::OSError& e) {
    return e.error();
  }
}

int32_t OperatingSystem::write_channel_bytes(int32_t channel,
                                             const types::ByteArray& data) {
  try {
    os::FSChannel* ch;
    {
      std::lock_guard<std::mutex> lock(machine.process->channel_mutex);
      ch=machine.process->retrieve_channel(channel);
    }
    std::vector<uint8_t> bytes=data.to_vector();
    ch->write(bytes);
    return 0;
  } catch(os::OSError& e) {
    return e.error();
  }
}

int32_t OperatingSystem::write_channel_bytes_at(int32_t channel,
                                                const types::ByteArray& data,
                                                int32_t record_number) {
  try {
    os::FSChannel* ch;
    {
      std::lock_guard<std::mutex> lock(machine.process->channel_mutex);
      ch=machine.process->retrieve_channel(channel);
    }
    ch->set_position(record_number);
    std::vector<uint8_t> bytes=data.to_vector();
    ch->write(bytes);
    return 0;
  } catch(os::OSError& e) {
    return e.error();
  }
}

int32_t OperatingSystem::open_shared_io_file(const types::String& filename,
                                             int32_t read_only,
                                             int32_t& channel) {
  try {
    std::string name=filename.str();
    size_t nul=name.find('\0');
    if(nul!=std::string::npos)
      name.resize(nul);
    std::string path=machine.task->full_path(name);
    os::FSChannel* ch=os::FSChannel::open_for_paged_io(path, read_only!=0);
    std::lock_guard<std::mutex> lock(machine.process->channel_mutex);
    channel=machine.process->assign_channel(ch);
    return 0;
  } catch(os::OSError& e) {
    return e.error();
  }
}

int32_t OperatingSystem::get_shared_page(int32_t channel, int32_t map_address,
                                         int32_t page_count,
                                         int32_t disk_page_number) {
  try {
    std::lock_guard<std::mutex> lock(machine.process->channel_mutex);
    os::FSChannel* ch=machine.process->retrieve_channel(channel);
    bool read_only=ch->read_only();
    if(!read_only) {
      os::FSFile* fs_file=ch->get_file();
      if(fs_file)
        fs_file->enable_memory_mapping();
    }
    int32_t memory_page=map_address>>10;
    int32_t total_pages=ch->page_count()-disk_page_number;
    machine.process->map_file(ch, memory_page, total_pages,
      disk_page_number, true, !read_only, false);
    return 0;
  } catch(os::OSError& e) {
    return e.error();
  }
}

int32_t OperatingSystem::create_task(int32_t entry_point, int32_t ac2_value,
                                     int32_t stack_size) {
  try {
    int32_t stack_base=machine.process->shared_start;
    os::OSTask* new_task=new os::OSTask(machine.process, entry_point,
      stack_base, stack_size, 0);
    if(machine.process->register_task(new_task)) {
      new_task->machine->ac[2]=ac2_value;
      new_task->launch();
    }
    return 0;
  } catch(os::OSError& e) {
    return e.error();
  }
}

int32_t OperatingSystem::await_console_interrupt() {
  while(!machine.task->halt) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  throw std::runtime_error("INTWT interrupted");
}

int32_t OperatingSystem::gshpt(int32_t& shared_start, int32_t& page_count) {
  shared_start=machine.process->shared_start;
  page_count=static_cast<int32_t>(machine.process->shared_pages.size());
  return 0;
}

int32_t OperatingSystem::sshpt(int32_t shared_start, int32_t page_count) {
  if(shared_start!=machine.process->shared_start)
    return os::OSError::OS_NOT_IMPLEMENTED;
  std::vector<os::OSSharedPageSource*> replacement(page_count, nullptr);
  for(int32_t i=0; i<page_count; i++)
    if(i<static_cast<int32_t>(machine.process->shared_pages.size()))
      replacement[i]=machine.process->shared_pages[i];
  machine.process->shared_pages=replacement;
  return 0;
}

int32_t OperatingSystem::get_current_time(int32_t& seconds, int32_t& minutes,
                                          int32_t& hours) {
  time_t now=time(nullptr);
  struct tm* tm=localtime(&now);
  seconds=tm->tm_sec;
  minutes=tm->tm_min;
  hours=tm->tm_hour;
  return 0;
}

int32_t OperatingSystem::create_ipc_file(const types::String& filename,
                                         int32_t local_port) {
  std::string name=filename.str();
  size_t nul=name.find('\0');
  if(nul!=std::string::npos)
    name.resize(nul);
  std::string path=machine.task->full_path(name);
  try {
    os::OS::global.register_service(machine.process, path, local_port);
    return 0;
  } catch(os::OSError& e) {
    return e.error();
  }
}

int32_t OperatingSystem::recreate_file(const types::String& filename) {
  // RECREATE is a no-op in the emulator (same as the emulated SYSCALL 0336)
  std::string name=filename.str();
  size_t nul=name.find('\0');
  if(nul!=std::string::npos)
    name.resize(nul);
  fprintf(stderr, "RECREATE: %s (no-op)\n", name.c_str());
  return 0;
}

} // namespace emu_types
