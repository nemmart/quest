#pragma once
#include "OSMessage.hpp"
#include <atomic>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <optional>


namespace os {
class OSQueue {
public:
  std::vector<OSMessage> messages;
  std::mutex mtx;
  std::condition_variable cv;
  std::atomic<bool> interrupted{false};

  void enqueue(const OSMessage& message) {
    std::lock_guard<std::mutex> lock(mtx);
    messages.push_back(message);
    cv.notify_all();
  }

  void interrupt() {
    interrupted.store(true);
    cv.notify_all();
  }

  std::optional<OSMessage> dequeue() {
    std::unique_lock<std::mutex> lock(mtx);
    while(true) {
      if(!messages.empty()) {
        OSMessage msg = messages.front();
        messages.erase(messages.begin());
        return msg;
      }
      if(interrupted.load())
        return std::nullopt;
      cv.wait_for(lock, std::chrono::seconds(1));
    }
  }
};

} // namespace os
