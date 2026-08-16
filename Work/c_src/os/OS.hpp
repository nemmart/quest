#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <mutex>


namespace os {
class OSProcess;
class OSQueue;
class OSMessage;
class FSChannel;
class FSStreamIO;

class OS {
public:
  static constexpr int32_t SUCCESS = 0;

  static OS global;

  int32_t next_pid;
  int32_t next_tid;
  std::unordered_map<int32_t, OSProcess*> pids;
  std::unordered_map<int32_t, OSQueue*> queues;
  std::unordered_map<std::string, int32_t> services;
  std::unordered_set<int32_t> connections;
  std::recursive_mutex mtx;

  OS();
  ~OS();

  static uint32_t aos_error(const std::string& name);
  static uint32_t aos_symbol(const std::string& name);

  FSChannel* open_file(OSProcess* process, const std::string& full_path, int32_t options);

  int32_t register_process(OSProcess* process);
  void unregister_process(OSProcess* process);
  int32_t next_TID();

  void register_service(OSProcess* process, const std::string& service, int32_t local_port);
  int32_t retrieve_service(const std::string& service);

  void send_message(const OSMessage& message);
  OSMessage* receive_message(int32_t pid);
  void interrupt_queue(int32_t pid);

  void connect(OSProcess* process, int32_t pid);

  void shutdown_all();
  bool has_processes();
};

} // namespace os
