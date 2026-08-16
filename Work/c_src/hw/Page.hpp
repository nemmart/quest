#pragma once
#include <cstdint>
#include "ReadWrite.hpp"


namespace hw {
// Designed to be subclassed. ArrayPage uses a byte array for code/private data.
// MappedPage could use mmap for shared file-backed pages.

class Page : public ReadWrite {
public:
  virtual ~Page() = default;

  virtual uint32_t read(uint32_t offset) = 0;
  virtual void write(uint32_t offset, uint32_t value) = 0;

  uint32_t read_byte(uint32_t offset) override {
    return read(offset);
  }

  uint32_t read_word(uint32_t offset) override {
    return read(offset*2+1) + (read(offset*2)<<8);
  }

  uint32_t read_wide(uint32_t offset) override {
    return read(offset*2+3) + (read(offset*2+2)<<8) + (read(offset*2+1)<<16) + (read(offset*2)<<24);
  }

  void write_byte(uint32_t offset, uint32_t value) override {
    write(offset, value);
  }

  void write_word(uint32_t offset, uint32_t value) override {
    write(offset*2, value>>8);
    write(offset*2+1, value);
  }

  void write_wide(uint32_t offset, uint32_t value) override {
    write(offset*2, value>>24);
    write(offset*2+1, value>>16);
    write(offset*2+2, value>>8);
    write(offset*2+3, value);
  }
};

} // namespace hw
