#include "MappedPage.hpp"




namespace os {
MappedPage::MappedPage(uint8_t* mapped_data, int32_t base_offset)
  : mapped_data(mapped_data), base_offset(base_offset) {}

uint32_t MappedPage::read(uint32_t offset) {
  return static_cast<uint32_t>(mapped_data[base_offset + offset]) & 0xFF;
}

void MappedPage::write(uint32_t offset, uint32_t value) {
  mapped_data[base_offset + offset] = static_cast<uint8_t>(value & 0xFF);
}

} // namespace os
