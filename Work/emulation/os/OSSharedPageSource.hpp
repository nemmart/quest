#pragma once
#include <cstdint>


namespace os {
class FSChannel;

class OSSharedPageSource {
public:
  FSChannel* channel;
  int32_t page_number;

  OSSharedPageSource(FSChannel* channel, int32_t page_number)
    : channel(channel), page_number(page_number) {}
};

} // namespace os
