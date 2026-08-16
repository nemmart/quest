#pragma once
#include <cstdint>
#include <vector>


namespace os {
class OSMessage {
public:
  int32_t origin_pid;
  int32_t origin_port;
  int32_t destination_pid;
  int32_t destination_port;
  int32_t user_flags;
  int32_t pointer;
  std::vector<int32_t> content;

  OSMessage(int32_t origin, int32_t destination, int32_t user_flags, int32_t pointer, const std::vector<int32_t>& content)
    : origin_pid(origin >> 16), origin_port(origin & 0xFFFF),
      destination_pid(destination >> 16), destination_port(destination & 0xFFFF),
      user_flags(user_flags), pointer(pointer), content(content) {}

  static OSMessage terminate_message(int32_t pid, int32_t destination_pid);

  int32_t origin() { return (origin_pid << 16) | origin_port; }
  int32_t destination() { return (destination_pid << 16) | destination_port; }
};

} // namespace os
