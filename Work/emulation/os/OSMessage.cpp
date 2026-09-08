#include "OSMessage.hpp"
#include "OS.hpp"




namespace os {
OSMessage OSMessage::terminate_message(int32_t pid, int32_t destination_pid) {
  return OSMessage(static_cast<int32_t>(OS::aos_symbol("?SPTM")),
                   destination_pid << 16,
                   static_cast<int32_t>(OS::aos_symbol("?TEXT")) + pid, 0, {});
}

} // namespace os
