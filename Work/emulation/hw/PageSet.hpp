#pragma once
#include <cstdint>
#include <string>
#include <stdexcept>
#include "ReadWrite.hpp"
#include "Block.hpp"


namespace hw {
class PageSet : public ReadWrite {
public:
  Block* blocks[4096];

  PageSet();
  virtual ~PageSet() = default;

  virtual void map_page(Page* page, uint32_t page_number);
  virtual void unmap_page(uint32_t page_number);
  virtual Page* find_page(uint32_t page_number);

  uint32_t read_byte(uint32_t address) override;
  uint32_t read_word(uint32_t address) override;
  uint32_t read_wide(uint32_t address) override;
  uint64_t read_quad(uint32_t address);
  void write_byte(uint32_t address, uint32_t value) override;
  void write_word(uint32_t address, uint32_t value) override;
  void write_wide(uint32_t address, uint32_t value) override;
  void write_quad(uint32_t address, uint64_t value);
};

} // namespace hw
