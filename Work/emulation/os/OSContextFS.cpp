#include "OSContextFS.hpp"
#include "../hw/Lockstep.hpp"
#include "OSProcess.hpp"
#include "OSTask.hpp"
#include "OSError.hpp"
#include "OS.hpp"
#include "FSChannel.hpp"
#include "FSStreamIO.hpp"
#include "../hw/Memory.hpp"
#include "../hw/Machine.hpp"
#include <cstdio>
#include <stdexcept>
#include <vector>
#include <mutex>
#include <cstdlib>




namespace os {
using namespace hw;

// TEMPORARY (queue-instruction reachability experiment, Aug 2026):
// QUEST_FAIL_OPEN="<file_substr>" makes ?OPEN_FILE fail with
// FS - file not found for the QUEST *client* processes only, leaving
// QUEST_SERVER able to open everything. Drives ?LIB_ERROR (and hence
// I.FREEW/I.ALLOC) on demand. Remove once the experiment is done.
// NOTE: "QUEST" is a prefix of "QUEST_SERVER" - the server must be
// excluded explicitly or it fails too.
static bool fail_open(const std::string& label, const std::string& file) {
  const char* want = getenv("QUEST_FAIL_OPEN");
  if(want == nullptr || label.compare(0, 5, "QUEST") != 0 ||
     label == "QUEST_SERVER")
    return false;
  return file.find(want) != std::string::npos;
}

OSContextFS::OSContextFS(OSProcess* p, OSTask* t, Memory* m, Machine* mc)
  : OSContext(p, t, m, mc) {}

int32_t OSContextFS::dispatch_system_call(int32_t call) {
  switch(call) {
    case OPEN:   return OPEN_call();
    case CLOSE:  return CLOSE_call();
    case READ:   return READ_call();
    case WRITE:  return WRITE_call();
    case UPDATE: return UPDATE_call();
  }
  throw std::runtime_error("Dispatch system call - missing case");
}

int32_t OSContextFS::OPEN_call() {
  int32_t options = read_packet_word("?ISTI");
  read_packet_word("?ISTO"); // file_type - unused
  int32_t block_size = read_packet_word("?IMRS");
  int32_t buffer_pointer = read_packet_wide("?IBAD");
  int32_t density_mode = read_packet_word("?IRES");
  int32_t record_length = read_packet_word("?IRCL");
  int32_t name_pointer = read_packet_wide("?IFNP");
  std::string file = full_path(read_string(name_pointer));
  int32_t slot = -1;

  if(process->system_call_logging) {
    printf("\nOPEN (fs op):\n");
    printf("   (fs op) options = %04X\n", options);
    printf("   (fs op) file type = %04X\n", block_size);
    printf("   (fs op) block size = %04X\n", density_mode);
    printf("   (fs op) record length = %04X\n", record_length);
    printf("   (fs op) buffer pointer = %08X\n", buffer_pointer);
    printf("   (fs op) name pointer = %08X\n", name_pointer);
    printf("   (fs op) file = %s\n", file.c_str());
  }

  if(fail_open(process->instance_label, file)) {
    fprintf(stderr, "FAIL_OPEN: %s denied %s\n",
            process->instance_label.c_str(), file.c_str());
    return OSError::FS_FILE_NOT_FOUND;
  }

  try {
    std::lock_guard<std::mutex> lock(process->channel_mutex);
    slot = process->assign_channel(OS::global.open_file(process, file, options));
  }
  catch(OSError& error) {
    return error.error();
  }
  if(process->system_call_logging)
    printf("Assigning channel: %d\n", slot);
  write_packet_word("?ICH", slot);
  return SUCCESS;
}

int32_t OSContextFS::CLOSE_call() {
  int32_t channel = read_packet_word("?ICH");

  if(process->system_call_logging)
    printf("\nCLOSE (fs cl):\n   (fs cl) channel = %04X\n\n", channel);

  try {
    std::lock_guard<std::mutex> lock(process->channel_mutex);
    if(!process->channels[channel])
      return OSError::OS_INVALID_CHANNEL_NUMBER;
    process->channels[channel]->close();
    process->channels[channel] = nullptr;
  }
  catch(OSError& error) {
    write_packet_word("?IRLR", 0);
    return error.error();
  }
  return SUCCESS;
}

int32_t OSContextFS::READ_call() {
  int32_t channel = read_packet_word("?ICH");
  int32_t options = read_packet_word("?ISTI");
  int32_t buffer_pointer = read_packet_wide("?IBAD");
  int32_t byte_count = read_packet_word("?IRCL");
  int32_t record_number = read_packet_wide("?IRNH");

  if(process->system_call_logging) {
    printf("\nREAD (fs rd):\n");
    printf("   (fs rd) channel = %04X\n", channel);
    printf("   (fs rd) options = %04X\n", options);
    printf("   (fs rd) record number = %d\n", record_number);
    printf("   (fs rd) buffer pointer = %08X\n", buffer_pointer);
    printf("   (fs rd) byte count = %04X\n", byte_count);
  }

  if(read_packet_flag("?ISTI", "?IPKL")) {
    int32_t packet = read_packet_wide("?ETSP");
    if(process->system_call_logging) {
      printf("READ EXTENSION\n");
      printf("   location = %08X\n", packet);
      if((packet & 0x7FFFFFFF) != 0) {
        printf("   options = %04X\n", read_packet_word("?ESFC", packet & 0x7FFFFFFF));
        printf("   rel pos = %04X\n", read_packet_word("?ESEP", packet & 0x7FFFFFFF));
        printf("   init pos = %04X\n", read_packet_word("?ESCR", packet & 0x7FFFFFFF));
      }
    }
  }

  std::vector<uint8_t> bytes(byte_count);
  try {
    FSChannel* file;
    int32_t amount;

    {
      std::lock_guard<std::mutex> lock(process->channel_mutex);
      if(!process->channels[channel])
        return OSError::OS_INVALID_CHANNEL_NUMBER;
      file = process->channels[channel];
    }
    if(read_packet_flag("?ISTI", "?IPST"))
      file->set_position(record_number);
    amount = file->read(bytes, !read_packet_flag("?ISTI", "?IBIN"));
    if(amount == -1)
      return static_cast<int32_t>(aos_error("EREOF"));
    if(amount > 0)
      write_byte_array(bytes, buffer_pointer, amount);
    write_packet_word("?IRLR", amount);
  }
  catch(OSError& error) {
    write_packet_word("?IRLR", 0);
    return error.error();
  }
  return SUCCESS;
}

// Lockstep spectator view: the clone's terminal ?WRITEs are mediated away,
// so its window would stay blank. Mirror the master's terminal output onto
// the clone's socket so the second window shows the live session. Host-side
// only -- no effect on emulated state or lockstep semantics. Echo failures
// (e.g. the spectator window was closed) permanently disable the echo
// rather than disturbing the master's write.
void OSContextFS::echo_to_clone_terminal(FSChannel* channel,
                                         const std::vector<uint8_t>& bytes) {
  static bool echo_failed = false;
  if(echo_failed || !hw::Lockstep::enabled ||
     process->lockstep_role != hw::Lockstep::MASTER || !channel->stream_io)
    return;
  for(auto& entry : OS::global.pids) {
    OSProcess* p = entry.second;
    if(!p || p->lockstep_role != hw::Lockstep::CLONE)
      continue;
    try {
      p->console()->write(bytes);
    }
    catch(std::exception&) {
      echo_failed = true;
    }
    return;
  }
}

int32_t OSContextFS::WRITE_call() {
  int32_t channel_number = read_packet_word("?ICH");
  int32_t options = read_packet_word("?ISTI");
  int32_t buffer_pointer = read_packet_wide("?IBAD");
  int32_t byte_count = read_packet_word("?IRCL");
  int32_t record_number = read_packet_wide("?IRNH");
  int32_t screen_options = 0, initial_position = 0, relative_position = 0;

  if(process->system_call_logging) {
    printf("\nWRITE (fs wt):\n");
    printf("   (fs wt) channel = %04X\n", channel_number);
    printf("   (fs wt) options = %04X\n", options);
    printf("   (fs wt) record number = %d\n", record_number);
    printf("   (fs wt) buffer pointer = %08X\n", buffer_pointer);
    printf("   (fs wt) byte count = %04X\n", byte_count);
    printf("%d\n", read_packet_flag("?ISTI", "?IPKL"));
  }

  if(read_packet_flag("?ISTI", "?IPKL")) {
    int32_t packet = read_packet_wide("?ETSP");
    screen_options = read_packet_word("?ESFC", packet & 0x7FFFFFFF);
    relative_position = read_packet_word("?ESEP", packet & 0x7FFFFFFF);
    initial_position = read_packet_word("?ESCR", packet & 0x7FFFFFFF);
    if(process->system_call_logging) {
      printf("WRITE EXTENSION\n");
      printf("   location = %08X\n", packet);
      printf("   options = %04X\n", screen_options);
      printf("   rel pos = %04X\n", relative_position);
      printf("   init pos = %04X\n", read_packet_word("?ESCR", packet));
    }
  }

  std::vector<uint8_t> bytes;
  if(byte_count == 0xFFFF) {
    bytes = read_byte_array(buffer_pointer, -1);
    printf("%s\n", read_string(buffer_pointer, -1).c_str());
    byte_count = static_cast<int32_t>(bytes.size());
  }
  else {
    bytes = read_byte_array(buffer_pointer, byte_count);
    printf("%s\n", read_string(buffer_pointer, byte_count).c_str());
  }

  try {
    FSChannel* channel;
    {
      std::lock_guard<std::mutex> lock(process->channel_mutex);
      channel = process->retrieve_channel(channel_number);
    }
    if((static_cast<uint32_t>(screen_options) & aos_symbol("?ESCP")) != 0) {
      std::vector<uint8_t> cursor = {020,
        static_cast<uint8_t>(initial_position >> 8),
        static_cast<uint8_t>(initial_position & 0xFF)};
      channel->write(cursor);
      echo_to_clone_terminal(channel, cursor);
    }
    channel->write(bytes);
    echo_to_clone_terminal(channel, bytes);
  }
  catch(OSError& error) {
    write_packet_word("?IRLR", 0);
    return error.error();
  }

  write_packet_word("?IRLR", byte_count);
  return SUCCESS;
}

int32_t OSContextFS::UPDATE_call() {
  printf("\nUPDATE:\n   channel = %04X\n\n", ac1);
  return SUCCESS;
}

} // namespace os
