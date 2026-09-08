#include "OS.hpp"
#include "AOSVSSymbols.hpp"
#include "OSError.hpp"
#include "OSProcess.hpp"
#include "OSTask.hpp"
#include "OSQueue.hpp"
#include "OSMessage.hpp"
#include "FSChannel.hpp"
#include "FSStreamIO.hpp"
#include <cstdio>
#include <stdexcept>
#include <algorithm>
#include <vector>




namespace os {
OS OS::global;

OS::OS() : next_pid(100), next_tid(100) {}

OS::~OS() {
  for(auto& [k, v] : queues)
    delete v;
}

uint32_t OS::aos_error(const std::string& name) {
  auto& map = AOSVSSymbols::symbols();
  auto it = map.find(name);
  if(it == map.end())
    throw std::runtime_error("Undefined AOS/VS symbol '" + name + "'");
  return it->second;
}

uint32_t OS::aos_symbol(const std::string& name) {
  return aos_error(name);
}

FSChannel* OS::open_file(OSProcess* process, const std::string& full_path, int32_t options) {
  if(full_path.size() > 0 && full_path[0] == '@') {
    std::string upper = full_path;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    if(upper == "@INPUT")
      return FSChannel::open_for_streamed_io(process->console(), FSChannel::READ_PERMISSION);
    else if(upper == "@OUTPUT")
      return FSChannel::open_for_streamed_io(process->console(), FSChannel::WRITE_PERMISSION);
    throw OSError(OSError::FS_FILE_NOT_FOUND);
  }
  return FSChannel::open_for_streamed_io(full_path, true);
}

int32_t OS::register_process(OSProcess* process) {
  std::lock_guard<std::recursive_mutex> lock(mtx);
  int32_t pid = next_pid++;
  pids[pid] = process;
  queues[pid] = new OSQueue();

  // Default label; Launch overrides with QUEST1/QUEST2 etc. when the same
  // program is being started more than once (dual-emulation runs).
  if(process->instance_label.empty())
    process->instance_label = process->program;
  return pid;
}

void OS::unregister_process(OSProcess* process) {
  std::lock_guard<std::recursive_mutex> lock(mtx);
  int32_t pid = process->pid;
  fprintf(stderr, "Unregistering process %d\n", pid);
  pids.erase(pid);
  auto qit = queues.find(pid);
  if(qit != queues.end()) {
    qit->second->interrupt();
    delete qit->second;
    queues.erase(qit);
  }

  // Remove services owned by this process
  std::vector<std::string> to_remove;
  for(auto& [name, val] : services) {
    if((val >> 16) == pid)
      to_remove.push_back(name);
  }
  for(auto& name : to_remove)
    services.erase(name);

  // Remove connections and send terminate messages
  std::vector<int32_t> conn_remove;
  for(auto conn : connections) {
    if((conn & 0xFFFF) == pid || (conn >> 16) == pid)
      conn_remove.push_back(conn);
  }
  for(auto conn : conn_remove) {
    connections.erase(conn);
    if((conn >> 16) == pid) {
      int32_t other = conn & 0xFFFF;
      fprintf(stderr, "message to %d\n", other);
      send_message(OSMessage::terminate_message(pid, other));
    }
  }
}

int32_t OS::next_TID() {
  std::lock_guard<std::recursive_mutex> lock(mtx);
  return next_tid++;
}

void OS::register_service(OSProcess* process, const std::string& service, int32_t local_port) {
  std::lock_guard<std::recursive_mutex> lock(mtx);
  if(services.count(service))
    throw OSError(OSError::OS_SERVICE_ALREADY_EXISTS);
  services[service] = (process->pid << 16) + local_port;
}

int32_t OS::retrieve_service(const std::string& service) {
  std::lock_guard<std::recursive_mutex> lock(mtx);
  auto it = services.find(service);
  if(it == services.end())
    throw OSError(OSError::OS_SERVICE_DOES_NOT_EXIST);
  return it->second;
}

void OS::send_message(const OSMessage& message) {
  std::lock_guard<std::recursive_mutex> lock(mtx);
  auto it = queues.find(message.destination_pid);
  if(it == queues.end())
    return;  // Process already exited
  it->second->enqueue(message);
}

OSMessage* OS::receive_message(int32_t pid) {
  OSQueue* queue = nullptr;
  {
    std::lock_guard<std::recursive_mutex> lock(mtx);
    auto it = queues.find(pid);
    if(it == queues.end())
      throw std::runtime_error("Invalid PID specified");
    queue = it->second;
  }
  auto result = queue->dequeue();
  if(!result.has_value())
    return nullptr;
  // Caller must manage lifetime
  return new OSMessage(result.value());
}

void OS::interrupt_queue(int32_t pid) {
  std::lock_guard<std::recursive_mutex> lock(mtx);
  auto it = queues.find(pid);
  if(it != queues.end())
    it->second->interrupt();
}

void OS::connect(OSProcess* process, int32_t pid) {
  std::lock_guard<std::recursive_mutex> lock(mtx);
  fprintf(stderr, "Connecting %d with %d\n", process->pid, pid);
  connections.insert((process->pid << 16) + pid);
  connections.insert((pid << 16) + process->pid);
}

void OS::shutdown_all() {
  std::lock_guard<std::recursive_mutex> lock(mtx);
  for(auto& [pid, process] : pids) {
    process->terminating = true;
    std::lock_guard<std::recursive_mutex> tlock(process->task_mutex);
    for(auto* t : process->tasks) {
      if(t) t->halt_task();
    }
  }
  for(auto& [pid, queue] : queues) {
    queue->interrupt();
  }
}

bool OS::has_processes() {
  std::lock_guard<std::recursive_mutex> lock(mtx);
  return !pids.empty();
}

} // namespace os
