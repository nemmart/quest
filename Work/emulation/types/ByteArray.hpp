// src/types/ByteArray.hpp
#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

namespace types {

class ByteArray {
public:
  virtual ~ByteArray() = default;

  virtual size_t size() const = 0;       // current content length
  virtual size_t capacity() const = 0;   // maximum buffer length
  bool empty() const { return size()==0; }

  virtual uint8_t operator[](size_t pos) const = 0;
  virtual void set_byte(size_t pos, uint8_t val) = 0;
  virtual void set_size(size_t len) = 0;

  virtual std::vector<uint8_t> to_vector() const {
    std::vector<uint8_t> result(size());
    for(size_t i=0; i<size(); i++)
      result[i]=(*this)[i];
    return result;
  }

  virtual void assign(const std::vector<uint8_t>& data) {
    set_size(data.size());
    for(size_t i=0; i<data.size(); i++)
      set_byte(i, data[i]);
  }

  virtual void assign(const uint8_t* data, size_t len) {
    set_size(len);
    for(size_t i=0; i<len; i++)
      set_byte(i, data[i]);
  }
};

} // namespace types
