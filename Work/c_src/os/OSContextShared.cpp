#include "OSContextShared.hpp"
#include "Trace.hpp"
#include "OSProcess.hpp"
#include "OSTask.hpp"
#include "OSError.hpp"
#include "OSSharedPageSource.hpp"
#include "FSChannel.hpp"
#include "FSFile.hpp"
#include "../hw/Memory.hpp"
#include "../hw/Machine.hpp"
#include <cstdio>
#include <stdexcept>




namespace os {
using namespace hw;

OSContextShared::OSContextShared(OSProcess* p, OSTask* t, Memory* m, Machine* mc)
  : OSContext(p, t, m, mc) {}

int32_t OSContextShared::dispatch_system_call(int32_t call) {
  switch(call) {
    case GSHPT: return GSHPT_call();
    case SSHPT: return SSHPT_call();
    case SOPEN: return SOPEN_call();
    case SCLOSE: return SCLOSE_call();
    case SPAGE: return SPAGE_call();
  }
  throw std::runtime_error("Dispatch system call - missing case");
}

int32_t OSContextShared::GSHPT_call() {
  ac0 = process->shared_start;
  ac1 = static_cast<int32_t>(process->shared_pages.size());
  return SUCCESS;
}

int32_t OSContextShared::SSHPT_call() {
  if(ac0 != process->shared_start)
    return OSError::OS_NOT_IMPLEMENTED;
  std::vector<OSSharedPageSource*> replacement(ac1, nullptr);
  for(int32_t i = 0; i < ac1; i++)
    if(i < static_cast<int32_t>(process->shared_pages.size()))
      replacement[i] = process->shared_pages[i];
  process->shared_pages = replacement;
  return SUCCESS;
}

int32_t OSContextShared::SOPEN_call() {
  std::string file = full_path(read_string(ac0));
  int32_t slot = -1;

  printf("\nSOPEN:\n  ac0 = %s\n  ac1 = %d\n  ac2 = %d\n", file.c_str(), ac1, ac2);

  if(ac1 != -1)
    throw std::runtime_error("Unsupported SOPEN option");

  try {
    std::lock_guard<std::mutex> lock(process->channel_mutex);
    slot = process->assign_channel(FSChannel::open_for_paged_io(file, ac2 == 0));
    ac1 = slot;
  }
  catch(OSError& error) {
    return error.error();
  }

  printf("Assigned channel %d\n\n", slot);
  return SUCCESS;
}

// ?SCLOSE — "Closes a file previously opened for shared access." (AOS/VS)
//
//   AC0  reserved, 0
//   AC1  DG bit 0 (0x80000000) = 1 release the shared pages from the
//        caller's address space, 0 retain them;
//        DG bits 2-31 = the channel number returned by ?SOPEN
//   AC2  reserved, 0
//
// Used by ?FATAL at 0x7017F66B, walking its chain of open shared files
// and closing each with the release flag set (WIORI 1,0x80000000 at
// 0x7017F666) while the process dies. Note the DG bit numbering: bit 0
// is the MOST significant bit, so 0x80000000 is the flag, not a channel
// bit.
//
// Page release is a no-op here: the emulator drops the process's mapped
// pages at termination anyway, and ?FATAL is terminal by construction.
int32_t OSContextShared::SCLOSE_call() {
  bool release = (ac1 & static_cast<int32_t>(0x80000000)) != 0;
  int32_t channel = ac1 & 0x3FFFFFFF;   // DG bits 2-31

  printf("\n?SCLOSE:\n   channel = %d\n   release pages = %d\n\n",
         channel, release ? 1 : 0);

  std::lock_guard<std::mutex> lock(process->channel_mutex);
  if(channel < 0 || channel >= static_cast<int32_t>(process->channels.size()) ||
     !process->channels[channel])
    return static_cast<int32_t>(aos_symbol("ERFNO"));   // channel not open
  process->channels[channel]->close();
  process->channels[channel] = nullptr;
  return SUCCESS;
}

int32_t OSContextShared::SPAGE_call() {
  bool read_only = read_packet_flag("?PSTI", "?SPRO");
  int32_t page_count = read_packet_byte("?PSTI", OSContext::LOW) / 4;
  int32_t disk_page_number = read_packet_wide("?PRNH") / 2048;
  int32_t map_address = read_packet_wide("?PCAD");

  if(map_address % 1024 != 0)
    throw std::runtime_error("Non aligned SPAGE load");

  int32_t memory_page_number = map_address >> 10;

  printf("\nSPAGE:\n   read only = %d\n", read_only);
  printf("   memory destination = %08X\n", read_packet_wide("?PCAD"));
  printf("   page count (ignored) = %d\n", page_count);
  printf("   start page = %d\n\n", disk_page_number);

  try {
    std::lock_guard<std::mutex> lock(process->channel_mutex);
    FSChannel* channel = process->retrieve_channel(ac1);
    if(channel->read_only()) {
      read_only = true;
      printf("   Channel is read only!\n");
    }

    // Note: shared data files are held in memory (ArrayPages) and written
    // back at shutdown by FS::save_all, matching the Java emulator. We do
    // not memory-map them to the real files.
    {
      FSFile* fs_file = channel->get_file();
      if(fs_file && Trace::enabled("shared"))
        fs_file->enable_shared_trace(fs_file->get_path());
    }

    process->map_file(channel, memory_page_number,
      channel->page_count() - disk_page_number,
      disk_page_number, true, !read_only, false);
  }
  catch(OSError& error) {
    return error.error();
  }

  return SUCCESS;
}

} // namespace os
