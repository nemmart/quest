#pragma once
#include "../hw/Page.hpp"
#include <cstdint>


namespace os {
using namespace hw;

// C++ equivalent of Java's MappedPage backed by MappedByteBuffer.
// The mapped_data pointer points to the start of the entire mmap'd region;
// base_offset is the byte offset within that region for this 2048-byte page.

class MappedPage : public Page {
public:
  uint8_t* mapped_data;
  int32_t base_offset;

  MappedPage(uint8_t* mapped_data, int32_t base_offset);

  uint32_t read(uint32_t offset) override;
  void write(uint32_t offset, uint32_t value) override;
};

} // namespace os
