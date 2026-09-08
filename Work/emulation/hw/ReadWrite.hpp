#pragma once
#include <cstdint>


namespace hw {
class ReadWrite {
public:
  virtual ~ReadWrite() = default;
  virtual uint32_t read_byte(uint32_t address) = 0;
  virtual uint32_t read_word(uint32_t address) = 0;
  virtual uint32_t read_wide(uint32_t address) = 0;
  virtual void write_byte(uint32_t address, uint32_t value) = 0;
  virtual void write_word(uint32_t address, uint32_t value) = 0;
  virtual void write_wide(uint32_t address, uint32_t value) = 0;
};

} // namespace hw
