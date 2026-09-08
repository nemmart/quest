#pragma once
#include <cstdint>
#include <vector>


namespace os {
class FSPagedIO {
public:
  virtual ~FSPagedIO() = default;

  virtual std::vector<uint8_t> load_page(int32_t page_number) = 0;
};

} // namespace os
