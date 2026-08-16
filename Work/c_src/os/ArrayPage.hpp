#pragma once
#include "../hw/Page.hpp"
#include <cstdint>
#include <vector>
#include <functional>
#include <string>


namespace os {
using namespace hw;

class ArrayPage : public Page {
public:
  std::vector<uint8_t> bytes;
  std::function<void()> on_modified;

  // Set via FSFile::enable_shared_trace: when non-empty and the "shared"
  // trace type is enabled, every write emits a trace line attributed to
  // the process currently executing on the MachineThread worker.
  std::string trace_label;

  ArrayPage();
  explicit ArrayPage(const std::vector<uint8_t>& bytes);
  ArrayPage(const std::vector<uint8_t>& bytes, std::function<void()> on_modified);

  uint32_t read(uint32_t offset) override;
  void write(uint32_t offset, uint32_t value) override;

  void write_byte(uint32_t offset, uint32_t value) override;
  void write_word(uint32_t offset, uint32_t value) override;
  void write_wide(uint32_t offset, uint32_t value) override;
};

} // namespace os
