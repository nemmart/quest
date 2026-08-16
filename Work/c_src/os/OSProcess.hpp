#pragma once
#include "Launchable.hpp"
#include "OSSharedPageSource.hpp"
#include "../hw/NativeRegistry.hpp"
namespace hw { class AddressBook; }
#include <cstdint>
#include <string>
#include <vector>
#include <mutex>


namespace debug { class SymbolTable; }
namespace hw { class Memory; class Page; class PageSet; }

namespace os {
using namespace debug;
using namespace hw;

class FSChannel;
class FSStreamIO;
class OSTask;

class OSProcess : public Launchable {
public:
  static constexpr int32_t SUCCESS = 0;
  static constexpr int32_t SEGMENT_BASE = 0x70000000 >> 10;

  int32_t pid;
  int32_t lockstep_role = 0;          // hw::Lockstep role (set at launch)
  int32_t next_lockstep_ordinal = 0;
  // Mapper configuration (Mapper.md §3; Project 14): set at launch(),
  // applied to EVERY task's machine at OSTask construction — the
  // listener's mapper is configured too (with is_main_task=false), which
  // is what makes the redirect-time main-task assert meaningful.
  // mapper_book stays nullptr on the master and on non-QUEST programs:
  // master-vs-clone is a property of the configuration. The launch()-made
  // task is the main task; ?TASK-created tasks are not.
  hw::AddressBook* mapper_book = nullptr;
  // Per-process tid allocation keeps master and clone tid sequences
  // identical, so identity writebacks (?UIDSTAT ?UTID, ?TASK ?DID) stay
  // deterministic and those calls can remain LOCAL under lockstep.
  int32_t next_tid = 100;  // task creation ordinals for pairing
  std::string program;
  std::string instance_label;  // e.g. QUEST, QUEST2 - unique per live process
  std::string working_directory;
  SymbolTable* symbols;
  Memory* memory;
  hw::NativeRegistry native_registry;
  bool inject_armed = false;   // Project 5 fault injector: one shot per process (QUEST clients only)
  std::vector<OSTask*> tasks;
  std::vector<FSChannel*> channels;
  int32_t unshared_stop;
  int32_t shared_start;
  std::vector<OSSharedPageSource*> shared_pages;
  std::string start_directory;
  bool terminating;
  // ?DFRSCH / ?ERSCH / ?DRSCH state. The emulator does not actually gate
  // task scheduling on this; it exists so ?DFRSCH can report the prior
  // state truthfully, which is what SWAT.REX tests.
  bool rescheduling_disabled = false;
  bool system_call_logging;
  Page* page0;
  Page* page8;
  std::recursive_mutex task_mutex;
  std::mutex channel_mutex;

  OSProcess(const std::string& working_directory, const std::string& program);
  ~OSProcess();

  void launch(FSStreamIO* terminal) override;

  static std::string full_path(const std::string& working_directory, const std::string& filename);
  FSStreamIO* console();

  static Page* private_copy(Page* page);
  void map_file(FSChannel* channel, int32_t memory_page, int32_t page_count,
                int32_t file_page, bool shared, bool write_permission, bool execute_permission);
  Page* lockstep_shared_page(PageSet* ps, int32_t file_page,
                             bool write_perm, bool exec_perm);
  void mirror_server_mappings(Page* real, Page* copy);

  // Inverse of mirror_server_mappings, for terminal detach
  // (Lockstep::detach): re-map the plain real page over every SERVER
  // MirrorPage whose real page has a registered clone copy. Must run under
  // the world-pause (shared_write_mutex held), same as the forward
  // direction. Call BEFORE clearing the copy registry.
  static void unmirror_server_mappings();

  int32_t count_tasks();
  int32_t task_slot(OSTask* task);
  bool register_task(OSTask* task);
  void unregister_task(OSTask* task);

  int32_t next_channel();
  int32_t assign_channel(FSChannel* channel);
  FSChannel* retrieve_channel(int32_t channel);
};

} // namespace os
