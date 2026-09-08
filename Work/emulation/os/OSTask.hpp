#pragma once
#include <cstdint>
#include <string>
#include <thread>
#include <atomic>


namespace hw { class Machine; class Memory; }

namespace os {
using namespace hw;

class OSProcess;

class OSTask {
public:
  static constexpr int32_t SUCCESS = 0;
  static thread_local Machine* machine_for_backtrace;

  OSProcess* process;
  int32_t tid;
  std::string working_directory;
  int32_t start_address;
  int32_t wfp, wsp, wsb, wsl;
  int32_t stack_fault_handler;
  int32_t kill_address;
  Memory* memory;
  Machine* machine;
  std::thread thread;
  std::atomic<bool> halt;

  static void backtrace();

  // Full constructor (6 stack params)
  OSTask(OSProcess* process, int32_t start_address,
         int32_t wfp, int32_t wsp, int32_t wsb, int32_t wsl,
         int32_t stack_fault_handler);

  // Simplified constructor (base + size)
  OSTask(OSProcess* process, int32_t start_address,
         int32_t stack_base, int32_t stack_size,
         int32_t stack_fault_handler);

  ~OSTask();

  std::string full_path(const std::string& filename);
  void launch();
  void halt_task();
  void run();
  int32_t dispatch_system_call();
};

} // namespace os
